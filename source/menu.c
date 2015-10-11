#include "menu.h"
#include "draw.h"
#include "hid.h"
#include "fs.h"
#include "decryptor/features.h"


void DrawMenu(MenuInfo* currMenu, u32 index, bool fullDraw, bool subMenu) {
    u32 menublock_y0 = 40;
    u32 menublock_y1 = menublock_y0 + currMenu->n_entries * 10;
    
    if (fullDraw) { // draw full menu
        ClearTopScreen();
        DrawStringF(10, menublock_y0 - 20, "%s", currMenu->name);
        DrawStringF(10, menublock_y0 - 10, "==========================");
        DrawStringF(10, menublock_y1 +  0, "==========================");
        DrawStringF(10, menublock_y1 + 10, (subMenu) ? "A: Choose  B: Return" : "A: Choose");
        DrawStringF(10, menublock_y1 + 20, "START: Reboot");
        DrawStringF(10, SCREEN_HEIGHT - 20, "Remaining SD storage space: %llu MiB", RemainingStorageSpace() / 1024 / 1024);
        #ifdef WORKDIR
        DrawStringF(10, SCREEN_HEIGHT - 30, "Working directory: %s", WORKDIR);
        #endif
    }
    
    for (u32 i = 0; i < currMenu->n_entries; i++) { // draw menu entries / selection []
        char* name = currMenu->entries[i].name;
        DrawStringF(10, menublock_y0 + (i*10), (i == index) ? "[%s]" : " %s ", name);
    }
}

u32 ProcessEntry(MenuEntry* entry) {
    u32 pad_state;
    
    // unlock sequence for dangerous features
    if (entry->dangerous) {
        u32 unlockSequence[] = { BUTTON_LEFT, BUTTON_RIGHT, BUTTON_DOWN, BUTTON_UP, BUTTON_A };
        u32 unlockLvlMax = sizeof(unlockSequence) / sizeof(u32);
        u32 unlockLvl = 0;
        DebugClear();
        Debug("You selected \"%s\".", entry->name);
        Debug("This feature is potentially dangerous!");
        Debug("If you understand and wish to proceed, enter:");
        Debug("<Left>, <Right>, <Down>, <Up>, <A>");
        Debug("");
        Debug("(B to return, START to reboot)");
        while (true) {
            ShowProgress(unlockLvl, unlockLvlMax);
            if (unlockLvl == unlockLvlMax)
                break;
            pad_state = InputWait();
            if (!(pad_state & BUTTON_ANY))
                continue;
            else if (pad_state & unlockSequence[unlockLvl])
                unlockLvl++;
            else if (pad_state & (BUTTON_B | BUTTON_SELECT | BUTTON_START))
                break;
            else if (unlockLvl == 0 || !(pad_state & unlockSequence[unlockLvl-1]))
                unlockLvl = 0;
        }
        ShowProgress(0, 0);
        if (unlockLvl < unlockLvlMax)
            return pad_state;
    }
    
    // execute this entries function
    DebugClear();
    if (SetNand(entry->emunand) != 0) {
        Debug("%s: failed!", entry->name);
    } else {
        Debug("%s: %s!", entry->name, (*(entry->function))() == 0 ? "succeeded" : "failed");
    }
    Debug("");
    Debug("Press B to return, START to reboot.");
    while(!(pad_state = InputWait() & (BUTTON_B | BUTTON_SELECT | BUTTON_START)));
    
    // returns the last known pad_state
    return pad_state;
}

void ProcessMenu(MenuInfo* info, u32 nMenus) {
    MenuInfo mainMenu;
    MenuInfo* currMenu = &mainMenu;
    u32 index = 0;
    
    // build main menu structure from submenus
    memset(&mainMenu, 0x00, sizeof(MenuInfo));
    for (u32 i = 0; i < nMenus && i < 8; i++) {
        mainMenu.entries[i].name = info[i].name;
        mainMenu.entries[i].function = NULL;
    }
    #ifndef BUILD_NAME
    mainMenu.name = "Decrypt9 Main Menu";
    #else
    mainMenu.name = BUILD_NAME;
    #endif
    mainMenu.n_entries = nMenus;
    DrawMenu(&mainMenu, 0, true, false);
    
    // main processing loop
    while (true) {
        bool full_draw = true;
        u32 pad_state = InputWait();
        if ((pad_state & BUTTON_A) && (currMenu == &mainMenu)) {
            currMenu = info + index;
            index = 0;
        } else if (pad_state & BUTTON_A) {
            pad_state = ProcessEntry(currMenu->entries + index);
        } else if ((pad_state & BUTTON_B) && (currMenu != &mainMenu)) {
            index = currMenu - info;
            currMenu = &mainMenu;
        } else if (pad_state & BUTTON_DOWN) {
            index = (index == currMenu->n_entries - 1) ? 0 : index + 1;
            full_draw = false;
        } else if (pad_state & BUTTON_UP) {
            index = (index == 0) ? currMenu->n_entries - 1 : index - 1;
            full_draw = false;
        } else if ((pad_state & BUTTON_R1) && (currMenu != &mainMenu)) {
            if (++currMenu - info >= nMenus) currMenu = info;
            index = 0;
        } else if ((pad_state & BUTTON_L1) && (currMenu != &mainMenu)) {
            if (--currMenu < info) currMenu = info + nMenus - 1;
            index = 0;
        } else {
            full_draw = false;
        }
        if (pad_state & BUTTON_START)
            break;
        DrawMenu(currMenu, index, full_draw, currMenu != &mainMenu);
    }
}
