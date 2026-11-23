#include "Underglow.cpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <unordered_map>

namespace test {
constexpr size_t capacity=512;
std::array<Byte,capacity*lightStride> lamps{};
std::array<Byte,capacity*presetStride> styles{};
std::array<Byte,0x500> manager{};
std::array<bool,capacity> usedLamps{},usedStyles{};
std::array<float,capacity> levels{};
unsigned generation=0;
std::unordered_map<std::string,Id> namedStyles;
unsigned nativeNameSequence{};
size_t lampLimit=capacity;
struct Car { std::array<Byte,0x900> presentation{}; std::array<Byte,0x500> scene{}; };
std::array<Car,12> cars{};
std::array<bool,12> present{};
std::array<uint64_t,12> identity{};
template<class T> void put(Byte* p,size_t off,const T& value) { std::memcpy(p+off,&value,sizeof(value)); }
void live(Byte* p) { put(p,0,p);put(p,8,p); }
Id* createStyle(void*,Id* id,const SmallString* name,const Preset*) {
    *id=invalid;
    for(size_t i=0;i<capacity;++i) if(!usedStyles[i]) {
        usedStyles[i]=true;*id=static_cast<Id>(i);auto* p=styles.data()+i*presetStride;
        // Match native recycling: a freed node retains its old string. Empty
        // names don't overwrite it; nonempty names enter the native name map.
        live(p);put(p,0x30,uint32_t{0});put(p,0x34,*id);
        if(name->length) {
            const auto label=std::string(name->text,name->length)+std::to_string(++nativeNameSequence);
            auto [entry,inserted]=namedStyles.emplace(label,*id);assert(inserted);
            SmallString actual;actual.length=label.size();
            if(label.size()>=16) {const char* heap=entry->first.c_str();std::memcpy(actual.text,&heap,sizeof(heap));actual.capacity=label.size();}
            else std::memcpy(actual.text,label.c_str(),label.size()+1);
            put(p,0x10,actual);
        }
        break;
    }
    return id;
}
Id* createLamp(void*,Id* id) {
    *id=invalid;
    for(size_t i=0;i<lampLimit;++i) if(!usedLamps[i]) {
        usedLamps[i]=true;*id=static_cast<Id>(i);auto* p=lamps.data()+i*lightStride;
        std::memset(p,0,lightStride);live(p);put(p,0x30,invalid);put(p,0x38,++generation);break;
    }
    return id;
}
void assign(void*,Id lamp,Id style) {
    assert(usedLamps[lamp]&&usedStyles[style]);
    auto* p=styles.data()+style*presetStride;
    put(p,0x30,read<uint32_t>(p+0x30)+1);put(lamps.data()+lamp*lightStride,0x30,style);
}
void destroyLamp(void*,Id* id) {
    assert(*id<capacity&&usedLamps[*id]);
    auto style=read<Id>(lamps.data()+*id*lightStride+0x30);
    if(style!=invalid) {auto* p=styles.data()+style*presetStride;put(p,0x30,read<uint32_t>(p+0x30)-1);}
    usedLamps[*id]=false;levels[*id]=0;std::memset(lamps.data()+*id*lightStride,0,lightStride);*id=invalid;
}
void destroyStyle(void*,Id* id) {
    assert(*id<capacity&&usedStyles[*id]&&read<uint32_t>(styles.data()+*id*presetStride+0x30)==0);
    auto* p=styles.data()+*id*presetStride;const auto name=read<SmallString>(p+0x10);
    if(name.length) {
        const char* text=name.text;if(name.capacity>=16) std::memcpy(&text,name.text,sizeof(text));
        const auto found=namedStyles.find(std::string(text,name.length));assert(found!=namedStyles.end()&&found->second==*id);namedStyles.erase(found);
    }
    usedStyles[*id]=false;std::memset(p,0,16);*id=invalid;
}
void transform(void*,Id,const XMFLOAT4*) {}
void intensity(void*,Id id,float value,bool) { assert(id<capacity&&usedLamps[id]); levels[id]=value; }
void noop(void*) {}
void radioTick(void*,float) {}
unsigned char paused() { return 1; }
std::array<float,1024> spectrumBins{};
radio::Spectrum spectrum{};
int spectrumData(void*,int,void** data,unsigned* size,char*,int) {
    spectrum.length=1024;spectrum.channels=1;spectrum.bins[0]=spectrumBins.data();
    *data=&spectrum;*size=sizeof(spectrum);return 0;
}
#if UG_SUPPORT_HORIZON_RADIO_NETWORK
std::array<float,4> networkLevels{};
int networkBands(float* output) { std::copy(networkLevels.begin(),networkLevels.end(),output);return 1; }
#endif
void* copy(void* dest,const void* src,size_t size) { return std::memcpy(dest,src,size); }
void stub(size_t rva,const void* fn) {
    Byte code[]{0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xe0};std::memcpy(code+2,&fn,sizeof(fn));
    std::memcpy(game+rva,code,sizeof(code));
}
int lookup(const void* presentation,const float*,FH3MeetAppearance* out) {
    for(unsigned i=0;i<12;++i) if(present[i]&&presentation==cars[i].presentation.data()) {
        *out={sizeof(*out),104,i,0,identity[i],{.2f+float(i)/20,.8f,.4f}};return 1;
    }
    return 0;
}
size_t count() {return std::count(usedLamps.begin(),usedLamps.end(),true);}
void fill() {
    for(unsigned i=0;i<12;++i) {
        auto& c=cars[i];present[i]=true;identity[i]=i+1;
        put(c.presentation.data(),0,game+0x468e848);
        put(c.presentation.data(),0x10,c.scene.data());put(c.scene.data(),0x440,manager.data());
        XMFLOAT4X4 matrix;XMStoreFloat4x4(&matrix,XMMatrixTranslation(float(i)*4,280,-1444));
        put(c.presentation.data(),0x1a0,matrix);carLights(c.presentation.data());
    }
}
}

int main() {
    game=static_cast<Byte*>(VirtualAlloc(nullptr,imageSize,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));assert(game);
    test::stub(createPreset,reinterpret_cast<void*>(test::createStyle));test::stub(createLight,reinterpret_cast<void*>(test::createLamp));
    test::stub(destroyPreset,reinterpret_cast<void*>(test::destroyStyle));test::stub(destroyLight,reinterpret_cast<void*>(test::destroyLamp));
    test::stub(assignPreset,reinterpret_cast<void*>(test::assign));test::stub(setPosition,reinterpret_cast<void*>(test::transform));
    test::stub(setRotation,reinterpret_cast<void*>(test::transform));test::stub(setIntensity,reinterpret_cast<void*>(test::intensity));
    FH3ModApi host{};host.log=+[](FH3Mod*,const char*){};api=&host;
    originalCarLights=test::noop;originalCopyLightPacket=test::copy;
    static const FH3CarMeetsV1 provider{sizeof(provider),test::lookup};meetProvider=&provider;
    const Pool lamps{test::lamps.data(),test::lamps.data()+test::lamps.size()};
    const Pool styles{test::styles.data(),test::styles.data()+test::styles.size()};
    test::put(test::manager.data(),0,lamps);test::put(test::manager.data(),0x38,styles);
    test::put(test::manager.data(),0x50,test::manager.data());
    halfWidth=defaultHalfWidth;halfLength=defaultHalfLength;height=defaultHeight;range=defaultRange;power=defaultPower;
    frontInset=defaultFrontInset;rearInset=defaultRearInset;sideOffset=defaultSideOffset;sideTrim=defaultSideTrim;
    for(size_t i=Front;i<Front+zoneCount;++i) savedSettings.options[i]=1;
    savedSettings.sliders={.5f,1};
    Id foreignStyle,foreignLamp;SmallString name;Preset preset;
    test::createStyle(nullptr,&foreignStyle,&name,&preset);test::createLamp(nullptr,&foreignLamp);test::assign(nullptr,foreignLamp,foreignStyle);
    SmallString stale;std::memcpy(stale.text,"retired",8);stale.length=7;
    for(size_t i=1;i<test::capacity;++i) test::put(test::styles.data()+i*presetStride,0x10,stale);
    test::fill();assert(updateMeetLights(test::manager.data()));assert(test::count()==353);
    assert(meetWorlds[0].lights.front()==invalid);
    for(size_t slot=1;slot<12;++slot) for(Id id:meetWorlds[slot].lights) assert(id!=invalid);
    std::set<unsigned> colors;
    std::set<int> families;
    for(unsigned slot=1;slot<12;++slot) {
        const auto settings=meetSettings(meetSnapshots[slot].appearance);
        colors.insert(settings.garageRgb);
        families.insert((slot+104)%fx::kGroupCount);
        for(size_t zone=0;zone<zoneCount;++zone) assert(settings.options[Front+zone]==1);
        assert(settings.effects[0]>=0&&settings.effects[0]<fx::kEffectCount);
        assert(meetSettings(meetSnapshots[slot].appearance).effects==settings.effects);
    }
    assert(colors.size()==11&&families.size()==4);
    std::set<int> selectedEffects;
    for(unsigned identity=1;identity<=300;++identity) for(unsigned slot=1;slot<12;++slot) {
        FH3MeetAppearance a{sizeof(a),104,slot,0,identity,{.8f,.4f,.2f}};
        selectedEffects.insert(meetSettings(a).effects[0]);
    }
    assert(selectedEffects.size()==fx::kEffectCount);
    // Audio uses the same live radio frame as personal glow, on all four sides.
    auto audioSettings=meetSettings(meetSnapshots[4].appearance);
    audioSettings.effects.fill(fx::kEffectAudioPulse);
    radio::sample={};renderLights(test::manager.data(),styles,meetSnapshots[4].pose,audioSettings,GetTickCount64(),meetWorlds[4]);
    for(Id id:meetWorlds[4].lights) assert(test::levels[id]==0);
    radio::sample.last=GetTickCount64();
    for(size_t zone=0;zone<zoneCount;++zone) radio::sample.lvl[zone]=radio::sample.peak[zone]=1;
    renderLights(test::manager.data(),styles,meetSnapshots[4].pose,audioSettings,GetTickCount64(),meetWorlds[4]);
    for(size_t zone=0;zone<zoneCount;++zone) {
        bool illuminated=false;
        for(size_t i=zoneStarts[zone];i<zoneStarts[zone]+zoneSizes[zone];++i) illuminated|=test::levels[meetWorlds[4].lights[i]]>0;
        assert(illuminated);
    }
    // Daytime packet selection must include every car, never stop at the old 32-light limit.
    daylightReady=true;daylightManager=test::manager.data();daylightUntil=GetTickCount64()+10000;
    DaylightUpload upload{styles};daylightUpload=&upload;
    std::array<Byte,maxOwnedLights*lightPacketSize> packets{};
    std::array<Byte,lightPacketSize> packet{};
    for(unsigned slot=0;slot<test::capacity;++slot) if(test::usedStyles[slot]) {
        test::put(packet.data(),0,1.0f);test::put(packet.data(),0x20,slot);
        copyLightPacket(packets.data(),packet.data(),packet.size());
    }
    assert(upload.slotCount==352&&upload.packetCount==352);daylightUpload=nullptr;
    // Global Off removes all meet lights, without deleting another mod's lamp.
    modEnabled=false;assert(!updateMeetLights(test::manager.data()));assert(test::count()==1&&test::usedLamps[foreignLamp]);
    modEnabled=true;assert(updateMeetLights(test::manager.data())&&test::count()==353);
    test::present[11]=false;assert(updateMeetLights(test::manager.data())&&test::count()==321);
    // Missing provider / leaving the meet clears all cached cars on the next scene frame.
    meetProvider=nullptr;assert(!updateMeetLights(test::manager.data())&&test::count()==1);
    meetProvider=&provider;test::fill();assert(updateMeetLights(test::manager.data()));
    ++test::identity[1];carLights(test::cars[1].presentation.data());assert(updateMeetLights(test::manager.data())&&test::count()==353);
    modEnabled=false;updateMeetLights(test::manager.data());
    test::lampLimit=100;modEnabled=true;updateMeetLights(test::manager.data());assert(test::count()<=100);
    modEnabled=false;updateMeetLights(test::manager.data());assert(test::count()==1);
    assert(std::count(test::usedStyles.begin(),test::usedStyles.end(),true)==1);
    modEnabled=true;
    // The player keeps the exact equipped glow in the meet; no forced player neon.
    test::lampLimit=test::capacity;
    for(auto& owner:meetWorlds) owner.retryAfter=0; // Advance past the capacity-test backoff.
    std::array<Byte,0x7400> player{};
    std::array<Byte,0x100> playerManagerData{},session{};
    std::array<Byte,0x300> simulation{};
    std::array<Byte*,1> driven{player.data()};
    const std::array<uintptr_t,2> list{reinterpret_cast<uintptr_t>(driven.data()),reinterpret_cast<uintptr_t>(driven.data()+1)};
    test::put(game,playerManager,playerManagerData.data());test::put(playerManagerData.data(),0x50,session.data());
    test::put(session.data(),0x98,simulation.data());test::put(simulation.data(),0x298,list.data());
    test::put(player.data(),0,game+0x42e0650);test::put(player.data(),0x7398,1);
    test::put(player.data(),0x7378,player.data());test::put(player.data(),0x73a0,test::cars[0].presentation.data());
    snapshot=meetSnapshots[1].pose;snapshot.car=player.data();snapshot.presentation=test::cars[0].presentation.data();
    read(snapshot.presentation+0x1a0,snapshot.matrix);
    Settings equipped=savedSettings;equipped.lightsEnabled=true;
    meetProvider=nullptr;updateSceneLights(test::manager.data(),equipped,false);
    assert(test::count()==33&&test::levels[world.lights[0]]>0);
    meetProvider=&provider;test::fill();
    updateSceneLights(test::manager.data(),equipped,false);updateMeetLights(test::manager.data());
    assert(test::count()==385&&test::levels[world.lights[0]]>0);
    equipped.lightsEnabled=false;updateSceneLights(test::manager.data(),equipped,false);
    assert(meetWorlds[0].lights[0]==invalid&&test::levels[world.lights[0]]==0);
    equipped.lightsEnabled=true;updateSceneLights(test::manager.data(),equipped,false);
    assert(test::levels[world.lights[0]]>0);
    meetProvider=nullptr;updateMeetLights(test::manager.data());updateSceneLights(test::manager.data(),equipped,false);
    assert(test::count()==33&&test::levels[world.lights[0]]>0);
    removeLights(test::manager.data(),lamps,styles);assert(test::count()==1);
    // Engine reuse of one formerly owned preset must never delete its new
    // owner, even when other lamps from our batch remain alive.
    assert(makeLights(test::manager.data(),lamps,styles));
    const Id recycled=world.presets[0];
    test::destroyLamp(nullptr,&world.lights[0]);
    Id oldStyle=recycled;test::destroyStyle(nullptr,&oldStyle);
    SmallString foreignName;std::memcpy(foreignName.text,"OTHER",6);foreignName.length=5;
    Id reused;test::createStyle(nullptr,&reused,&foreignName,&preset);assert(reused==recycled);
    removeLights(test::manager.data(),lamps,styles);
    assert(test::usedStyles[reused]&&test::namedStyles.size()==1&&test::namedStyles.begin()->first.starts_with("OTHER"));
    assert(test::count()==1);
    test::destroyStyle(nullptr,&reused);
    assert(test::namedStyles.empty());
    // Run the production sampler with a meet UI marked as "paused" and its
    // free-roam station flags off. It must read live FFT, never hold a frame.
    std::vector<Byte> engine(0xaa000);
    std::array<Byte,0x100> radioManager{};
    test::put(game,radio::engineGlobal,engine.data());
    test::put(engine.data(),0xa9fe8,engine.data());
    radio::tap={};radio::tap.studio=engine.data();radio::tap.fft=engine.data();
    radio::tap.route=radio::Route::off;radio::tap.connected=true;
    radio::tap.nextLookup=GetTickCount64()+10000;radio::tap.peaks.fill(1);
    radio::originalTick=test::radioTick;radio::inPauseMenu=test::paused;
    test::stub(0x27c5100,reinterpret_cast<void*>(test::spectrumData));
    radio::samplePaused=true;radio::sample.last=GetTickCount64();radio::sample.lvl[0]=.75f;
    meetAudioUntil=GetTickCount64()+10000;
    radio::tick(radioManager.data(),.016f);
    assert(!radio::samplePaused&&radio::sample.lvl[0]==0);
    test::spectrumBins.fill(.8f);radio::tick(radioManager.data(),.016f);
    for(float level:radio::sample.lvl) assert(level>.1f);
    assert(radio::reportedMeetAudio);
    meetAudioUntil=0;const auto held=radio::sample.lvl;
    test::spectrumBins.fill(0);radio::tick(radioManager.data(),.016f);
    assert(radio::samplePaused&&radio::sample.lvl==held);
#if UG_SUPPORT_HORIZON_RADIO_NETWORK
    static const radio::HorizonRadioBandsV1 network{test::networkBands};
    radio::horizonRadio=&network;meetAudioUntil=GetTickCount64()+10000;
    radio::tick(radioManager.data(),.016f);assert(!radio::samplePaused&&radio::sample.lvl[0]==0);
    test::networkLevels.fill(.8f);radio::tick(radioManager.data(),.016f);
    for(float level:radio::sample.lvl) assert(level>.1f);
    radio::horizonRadio=nullptr;
#endif
    std::cout<<"PASS: recycled native preset names registered safely; foreign reused presets preserved; live meet FFT updates despite pause/station flags; normal pause frame hold preserved.\n";
    std::cout<<"PASS: 11 NPC cars/352 lamps plus exact player glow; distinct colors and all effect families; full daylight upload; disable/missing/exit cleanup; pointer identity changes; capacity rollback; foreign light preserved; player stays equipped or unlit inside and outside meet.\n";
    return 0;
}
