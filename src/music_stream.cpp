#include "music_stream.hpp"
#include <chrono>
#include <iostream>
#include <numeric>

namespace siege {
void MusicStream::start(const std::vector<std::filesystem::path>& paths,bool random,float crossfade){
    stop();playlist=paths;shuffle=random;fadeSeconds=clamp(crossfade,0,5);written=0;read=0;
    if(!playlist.empty()){running=true;worker=std::thread(&MusicStream::produce,this);}
}
void MusicStream::stop(){running=false;if(worker.joinable())worker.join();}
bool MusicStream::pop(float& left,float& right) noexcept {
    auto at=read.load(std::memory_order_relaxed);
    if(at==written.load(std::memory_order_acquire)){left=right=0;return false;}
    left=ring[size_t(at%capacity)*2];right=ring[size_t(at%capacity)*2+1];
    read.store(at+1,std::memory_order_release);return true;
}
void MusicStream::produce(){
    std::mt19937 random(std::random_device{}());
    std::vector<size_t> order(playlist.size());std::iota(order.begin(),order.end(),0);
    size_t cursor=order.size(),last=order.size();std::vector<bool> bad(order.size(),false);
    auto next=[&]()->std::unique_ptr<AudioDecoder>{
        // A request may straddle a reshuffle; allow a full new cycle after it.
        for(size_t tries=0;tries<order.size()*2;++tries){
            if(cursor==order.size()){
                cursor=0;if(shuffle)std::shuffle(order.begin(),order.end(),random);
                if(shuffle&&order.size()>1&&order[0]==last)std::swap(order[0],order[1]);
            }
            auto index=order[cursor++];if(bad[index])continue;
            auto decoder=std::make_unique<AudioDecoder>();
            if(decoder->open(playlist[index])){last=index;return decoder;}
            bad[index]=true;std::cerr<<"Music: skipping undecodable track "<<playlist[index]<<'\n';
        }
        return {};
    };
    auto current=next();if(!current)return;
    // Hold back a small tail until actual decoder EOF. MP3 length estimates can
    // include cover-art bytes; they must not determine the transition boundary.
    const size_t fadeFrames=size_t(fadeSeconds*audioRate);
    std::vector<float> pending;pending.reserve(fadeFrames*2+2048);
    std::vector<float> incoming(fadeFrames*2);
    std::array<float,2048> decoded{};
    auto emit=[&](const float* samples,size_t frames){
        size_t sent=0;
        while(sent<frames&&running){
            auto w=written.load(std::memory_order_relaxed),r=read.load(std::memory_order_acquire);
            size_t room=capacity-size_t(w-r);
            if(!room){std::this_thread::sleep_for(std::chrono::milliseconds(2));continue;}
            size_t count=std::min(room,frames-sent);
            for(size_t i=0;i<count;++i){size_t at=size_t((w+i)%capacity)*2;ring[at]=samples[(sent+i)*2];ring[at+1]=samples[(sent+i)*2+1];}
            written.store(w+count,std::memory_order_release);sent+=count;
        }
    };
    while(running){
        int count=current->read(decoded.data(),1024);
        if(count>0){
            pending.insert(pending.end(),decoded.begin(),decoded.begin()+count*2);
            if(pending.size()/2>fadeFrames){
                size_t available=pending.size()/2-fadeFrames;
                emit(pending.data(),available);
                pending.erase(pending.begin(),pending.begin()+available*2);
            }
            continue;
        }
        auto following=next();
        if(!following){emit(pending.data(),pending.size()/2);return;}
        size_t tail=pending.size()/2;
        size_t head=size_t(following->read(incoming.data(),int(tail)));
        size_t overlap=std::min(tail,head),prefix=tail-overlap;
        for(size_t i=0;i<overlap;++i){
            float blend=float(i+1)/float(overlap);
            for(int c=0;c<2;++c){auto at=(prefix+i)*2+c;pending[at]=pending[at]*(1-blend)+incoming[i*2+c]*blend;}
        }
        emit(pending.data(),tail);pending.clear();current=std::move(following);
        // If a decoder opens but produces no frames, avoid a hot retry loop.
        if(!tail&&!head)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
}
