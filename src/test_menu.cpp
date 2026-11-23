#define main meetRegressionTests
#include "test_meet_lights.cpp"
#undef main

int noticeRefreshes{};
void* expectedNoticeModel{};
void notifyTest(void* model,void*) { assert(model==expectedNoticeModel); ++noticeRefreshes; }
bool buttonsTest(void*,int,bool) { return false; }
void selectionTest(void*,void*,void* value) { std::memcpy(const_cast<Byte*>(selectedRowAddress),value,4); }
void settingTest(FH3Mod*,const char*,int) {}
int acceptedLoadChanges{}, nativeLoadSetters{};
void loadChanged(void*,const char*,int on) {
    std::ofstream file(std::filesystem::path(api->azimuth_dir)/L"fh3-mods/platform-disabled.txt");
    if (!on) file<<"Underglow.dll\n";
    ++acceptedLoadChanges;
}
void unusedRowSetter(void*,void*,void*) { ++nativeLoadSetters; }
int main() {
    static FH3ModApi host{};
    static auto path=std::filesystem::current_path()/"menu-fixture";
    std::filesystem::create_directories(path/"fh3-mods");
    const auto name=path.wstring(); host.azimuth_dir=name.c_str();
    host.log=+[](FH3Mod*,const char*){};host.set_setting=settingTest;api=&host;
    assert(initializeIni(name.c_str()));
    std::vector<Byte> memory(0x26b000);
    platform=memory.data();platformMenu=&core::platform_menu::menus[0];
    std::array<Byte,0xb0*3> entries{};
    void* page=reinterpret_cast<void*>(10);
    mod=reinterpret_cast<FH3Mod*>(123);
    test::put(entries.data(),0xb0*2+0xa0,mod);
    test::put(memory.data(),platformMenu->entries,entries.data());
    test::put(memory.data(),platformMenu->entries+8,entries.data()+entries.size());
    test::put(memory.data(),platformMenu->installed_page,page);
    test::put(memory.data(),platformMenu->view,3);
    test::put(memory.data(),platformMenu->in_hub,Byte{1});
    selectedRowAddress=memory.data()+0x2362a8;
    menuReady=true;originalMenuButtons=buttonsTest;originalSelectionSet=selectionTest;notifyProperty=notifyTest;
    savedSettings={};savedSettings.options[Front]=savedSettings.options[Rear]=1;draftSettings=savedSettings;
    changeOption(&draftSettings.options[Front],"Front",0);
    assert(draftSettings.options[Front]==0 && savedSettings.options[Front]==1);
    int next=1;menuSelectionSet(nullptr,nullptr,&next);
    assert(draftSettings.options[Front]==1 && savedSettings.options[Front]==1 && !draftRow.page);
    changeOption(&draftSettings.options[Front],"Front",0);
    menuButtons(page,acceptButton,false);assert(savedSettings.options[Front]==1);
    menuButtons(page,acceptButton,true);assert(savedSettings.options[Front]==0);
    assert(getInt(L"Front",1,0,1)==0);
    changeOption(&draftSettings.options[Rear],"Rear",0);
    menuButtons(page,backButton,true);assert(draftSettings.options[Rear]==1);
    changeOption(&draftSettings.options[Front],"Front",1);
    test::put(memory.data(),platformMenu->view,0);
    bool animate{};settingsForFrame(animate);assert(draftSettings.options[Front]==0);
    // More than one deque block, and Underglow is not entry zero.
    std::array<Byte,0xd0*19> first{},second{};
    std::array<Byte*,2> blocks{first.data(),second.data()};
    test::put(memory.data(),platformMenu->rows+8,blocks.data());
    test::put(memory.data(),platformMenu->rows+16,blocks.data()+blocks.size());
    test::put(memory.data(),platformMenu->rows+32,size_t{2});
    test::put(memory.data(),platformMenu->rows+40,size_t{20});
    auto* row=second.data();const char key[]="Mod_2";
    row[8]=10;std::memcpy(row+9,key,sizeof(key));
    test::put(row,0x78,reinterpret_cast<void*>(9));test::put(row,0x80,uintptr_t{2});
    test::put(row,0x68,1);test::put(row,0x6c,1);
    assert(loadNoticeRow()==row);
    noticeBinding={reinterpret_cast<void*>(7),reinterpret_cast<void*>(8)};
    test::put(memory.data(),0x234e00,noticeBinding.model);
    test::put(memory.data(),0x235b10,noticeBinding.descriptor);
    for(int cycle=0;cycle<3;++cycle) for(int desired:{0,1}) {
        // Reopen the list/settings with a replacement binding model before
        // every F9. A cached row-setter model must never be notified again.
        expectedNoticeModel=reinterpret_cast<void*>(uintptr_t{100}+noticeRefreshes);
        test::put(memory.data(),0x234e00,expectedNoticeModel);
        test::put(row,0x68,desired);
        assert(read<int>(row+0x68)!=read<int>(row+0x6c));
        // A failed/deferred F9 does not advance appliedLoadNotice.
        appliedLoadNotice=1-desired;updateLoadNotice();
        assert(read<int>(row+0x68)!=read<int>(row+0x6c));
        appliedLoadNotice=desired;updateLoadNotice();
        assert(read<int>(row+0x68)==read<int>(row+0x6c));
    }
    assert(noticeRefreshes==6);
    appliedLoadNotice=-1;test::put(row,0x68,0);updateLoadNotice();
    assert(read<int>(row+0x6c)==1); // HotReload=0 preserves restart baseline.
    // A changed Load preview shows the notice without persisting. An already
    // accepted pending request must survive an opposite unaccepted preview.
    originalRowSet=unusedRowSetter;
    test::put(memory.data(),platformMenu->view,3);
    test::put(row,0x78,&loadChanged);
    test::put(row,0x88,reinterpret_cast<void*>(7));
    test::put(row,0x68,1);test::put(row,0x6c,1);
    loadChanged(nullptr,nullptr,1);acceptedLoadChanges=0;appliedLoadNotice=1;
    std::array<Byte,0x30> boxed{};
    const auto chooseLoad=[&](int selected) {
        test::put(boxed.data(),0x28,selected);
        menuRowSet(reinterpret_cast<void*>(7),expectedNoticeModel,boxed.data());
    };
    const auto pending=[&] { return read<int>(row+0x68)!=read<int>(row+0x6c); };
    chooseLoad(0);
    assert(pending() && loaderRequestedOn(name.c_str()) && acceptedLoadChanges==0);
    next=2;menuSelectionSet(nullptr,nullptr,&next);
    assert(!loadPreview.row && read<int>(row+0x68)==1 && loaderRequestedOn(name.c_str()) && !pending());
    chooseLoad(0);menuButtons(page,backButton,true);
    assert(!loadPreview.row && read<int>(row+0x68)==1 && !pending());
    chooseLoad(0);menuButtons(page,acceptButton,false);
    assert(pending() && acceptedLoadChanges==0);
    menuButtons(page,acceptButton,true);
    assert(pending() && acceptedLoadChanges==1 && !loaderRequestedOn(name.c_str()));
    chooseLoad(1);assert(pending() && !loaderRequestedOn(name.c_str()));
    menuButtons(page,backButton,true);assert(pending() && read<int>(row+0x68)==0);
    appliedLoadNotice=0;updateLoadNotice();assert(!pending());
    chooseLoad(1);assert(pending() && !loaderRequestedOn(name.c_str()));
    menuButtons(page,backButton,true);
    assert(!pending() && read<int>(row+0x68)==0 && !loaderRequestedOn(name.c_str()));
    chooseLoad(1);assert(pending());
    menuButtons(page,acceptButton,true);assert(pending() && loaderRequestedOn(name.c_str()));
    appliedLoadNotice=1;updateLoadNotice();assert(!pending());
    assert(acceptedLoadChanges==2 && nativeLoadSetters==0);
    // Restart-only mode still waits for A, but its accepted value remains
    // pending against the boot state; no F9 baseline is invented.
    appliedLoadNotice=-1;chooseLoad(0);assert(pending());
    menuButtons(page,acceptButton,true);assert(pending());
    for (const auto part : {Front, Rear, Left, Right}) {
        const int before=savedSettings.options[part], preview=1-before;
        changeOption(&draftSettings.options[part],optionRows[part].key,preview);
        assert(draftSettings.options[part]==preview && savedSettings.options[part]==before);
        next=selectedRow()+1;menuSelectionSet(nullptr,nullptr,&next);
        assert(draftSettings.options[part]==before && savedSettings.options[part]==before);
        changeOption(&draftSettings.options[part],optionRows[part].key,preview);
        menuButtons(page,acceptButton,true);
        assert(savedSettings.options[part]==preview && getInt(optionRows[part].iniKey,before,0,1)==preview);
        changeOption(&draftSettings.options[part],optionRows[part].key,before);
        menuButtons(page,backButton,true);
        assert(draftSettings.options[part]==preview && savedSettings.options[part]==preview);
    }
    // Literal user sequence: leave/reopen the mod page through MODS, then
    // apply the accepted state in free roam. Repeat in both directions.
    for (int active : {1,0}) {
        appliedLoadNotice=active;
        test::put(row,0x68,active);test::put(row,0x6c,active);
        loadChanged(nullptr,nullptr,active);
        test::put(memory.data(),platformMenu->view,3);
        chooseLoad(1-active);
        assert(pending() && loaderRequestedOn(name.c_str())==(active!=0));
        menuButtons(page,backButton,true);
        test::put(memory.data(),platformMenu->view,0); // MODS list
        assert(!pending() && read<int>(row+0x68)==active);
        test::put(memory.data(),platformMenu->view,3); // reopen mod
        assert(!pending() && read<int>(row+0x68)==active);
        chooseLoad(1-active);menuButtons(page,acceptButton,true);
        assert(pending() && loaderRequestedOn(name.c_str())==(active==0));
        menuButtons(page,backButton,true);
        test::put(memory.data(),platformMenu->view,0);
        assert(pending()); // accepted change survives in MODS list
        test::put(memory.data(),platformMenu->in_hub,Byte{0}); // free roam/F9
        appliedLoadNotice=1-active;
        test::put(memory.data(),platformMenu->in_hub,Byte{1});
        expectedNoticeModel=reinterpret_cast<void*>(uintptr_t{500}+active);
        test::put(memory.data(),0x234e00,expectedNoticeModel);
        updateLoadNotice();assert(!pending());
        test::put(memory.data(),platformMenu->view,3);
        assert(!pending() && read<int>(row+0x68)==1-active);
    }
    std::cout<<"PASS: exact On/Off -> preview notice -> Back -> MODS list/reopen restore; Accept retains notice in both views; free-roam F9 success clears it on return, in both directions.\n";
    std::cout<<"PASS: all four Front/Rear/Left/Right controls preview, restore on row exit or B, and persist only on A.\n";
    std::cout<<"PASS: Load On/Off preview shows a notice without persisting; row exit/B restore and clear preview-only notices symmetrically; A persists and keeps notice until successful F9; opposite preview preserves an existing accepted pending request; HotReload=0 keeps restart behavior.\n";
    std::cout<<"PASS: MODS previews roll back on row/page exit and B; only pressed A saves; repeated F9 notice baselines track success only, including a non-first mod in a second deque block.\n";
}
