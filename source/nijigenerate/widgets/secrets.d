/*
    Copyright © 2020-2023, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.widgets.secrets;

bool isTransMonthOfVisibility;
static this() {
    import std.datetime : Date, SysTime, Clock, Month;
    auto time = Clock.currTime();

    isTransMonthOfVisibility = time.month == Month.nov;

}