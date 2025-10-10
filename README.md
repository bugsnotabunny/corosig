# 🐟 COROSIG 🐟

Corosig (properly spelled as \[karа́s'ik\]) is a crossplatform (POSIX) library for using C++20 coroutines safely inside C library signal handlers.

## Why signal handlers are ✨different✨?

In POSIX, signal handlers are very restrictive environments. There is not a lot of things one can do safely inside it. There should be no allocations, syscalls which are not marked and calls to global objects must be kept to the minimum since they might be invalidated by the moment sighandler is entered.

Most of the time when signal is received it is possible to just set some flag somewhere, exit handler and after that handle occured signal somewhere in main event loop. This aproach is recommended, since it is much easier to follow.

## When returning to main loop is not possible?

Returning to main loop is not possible if a signal was raised by your implementation due to some horrific state occuring inside your program (such as std::terminate, std::abort, assertion, zero division, etc). If something scary has happened, then you cannot return to main loop, since there are invalidated objects somewhere. And in most cases you cannot know where, so there is noone to be trusted. This exact moment you should stay inside sighandler, send valuable diagnosing data (such as logs) somewhere they can be persisted for further studying.

And as any IO operation, this is faster and fancier when you do it asynchronously.

## Brief API overview

Library is within it's very early stage of development and interfaces may change in future

Currently, you have to do this in order to get access to coroutines in SIGFPE handler:

```cpp
corosig::Fut<void> sighandler(int sig) noexcept {
    // here you can co_await and do any other nasty stuff
}

int main() {
    // in bytes, how much stack space can be used
    // to allocate coroutine frames here
    constexpr auto REACTOR_MEMORY = 8000;
    corosig::set_sighandler<REACTOR_MEMORY, sighandler>(SIGFPE);

    return 0;
}
```

## About Windows

Sighandlers in Winapi are a psyop. They are even more restrictive than POSIX' ones since they do not allow ANY syscalls inside them. This means no IO at all. Set some flags and exit. That's it.

However, Winapi has SEH which may be useful for the same thing signals are used in POSIX: crash handling. It is possible that in future library will become more crossplatform via some wrapper around setting sighandler vs setting SEH.

But currently I am not even sure if such thing as async IO during crash handling is going to be useful for Windows programmers since I am not a Windows programmer at all.
