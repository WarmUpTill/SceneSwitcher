/*
 *  SceneSwitcher.cpp
 *  SceneSwitcher
 *
 *  Created by Till Wickenheiser on 25.06.16.
 *  Copyright © 2016 WarmUpTill. All rights reserved.
 *
 */
#include <iostream>
#include "SceneSwitcher.hpp"
#ifdef 	__APPLE__ //&& __MACH__
//do stuff for mac
#endif
#ifdef __WINDOWS__
//do stuff for windows
//this is so cool ! :D
string GetActiveWindowTitle()
{
    char wnd_title[256];
    //get handle of currently active window
    HWND hwnd = GetForegroundWindow();
    GetWindowTextA(hwnd, wnd_title, sizeof(wnd_title));
    return wnd_title;
}
#endif


void HelloWorldPriv(const char * s)
{
    std::string cmd = "osascript -e 'tell application \"System Events\"' -e 'set frontApp to name of first application process whose frontmost is true' -e 'end tell'";
    system(cmd.c_str());
	std::cout << s << std::endl;
};


//OSErr err;
//CGSValue titleValue;
//char *title;
//CGSConnection connection = _CGSDefaultConnection();
//int windowCount, *windows, i;
//
//NSCountWindows(&windowCount);
//windows = malloc(windowCount * sizeof(*windows));
//if (windows) {
//    NSWindowList(windowCount, windows);
//    for (i=0; i < windowCount; ++i) {
//        err = CGSGetWindowProperty(connection, windows[i],
//                                   CGSCreateCStringNoCopy("kCGSWindowTitle"),
//                                   &titleValue);
//        title = CGSCStringValue(titleValue);
//    }
//    free(windows);
//}
//
//Int getActiveWindowOSX(){
//    NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
//    NSApplication *myApp;
//    
//    
//    myApp=[NSApplication sharedApplication];
//    
//    NSWindow *myWindow = [myApp keyWindow];
//    
//    [pool release];
//    return NWSindow;
//};
