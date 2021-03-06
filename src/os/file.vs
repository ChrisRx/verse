#import "os"
#import "syscall"

Stdout := 1;
Stderr := 2;

O_CREAT     :=     0o100;
O_EXCL      :=     0o200;
O_NOCTTY    :=     0o400;
O_TRUNC     :=    0o1000;
O_APPEND    :=    0o2000;
O_NONBLOCK  :=    0o4000;
O_DSYNC     :=   0o10000;
O_SYNC      := 0o4010000;
O_RSYNC     := 0o4010000;
O_DIRECTORY :=  0o200000;
O_NOFOLLOW  :=  0o400000;
O_CLOEXEC   := 0o2000000;

O_ASYNC     :=    0o20000;
O_DIRECT    :=    0o40000;
O_LARGEFILE :=   0o100000;
O_NOATIME   :=  0o1000000;
O_PATH      := 0o10000000;
O_TMPFILE   := 0o20200000;
O_NDELAY    := O_NONBLOCK;

O_RDONLY := 0o0;
O_WRONLY := 0o1;
O_RDWR   := 0o2;

FD_CLOEXEC := 1;

F_DUPFD := 0;
F_GETFD := 1;
F_SETFD := 2;
F_GETFL := 3;
F_SETFL := 4;

F_SETOWN := 8;
F_GETOWN := 9;
F_SETSIG := 10;
F_GETSIG := 11;

F_GETLK  := 12;
F_SETLK  := 13;
F_SETLKW := 14;

F_SETOWN_EX := 15;
F_GETOWN_EX := 16;

F_GETOWNER_UIDS := 17;

AT_FDCWD            := -100;
AT_SYMLINK_NOFOLLOW := 0x100;
AT_REMOVEDIR        := 0x200;
AT_SYMLINK_FOLLOW   := 0x400;
AT_EACCESS          := 0x200;

fn open(path: string, flags: int, mode: int) -> int {
    fd := syscall.syscall3(syscall.sys_open, path.bytes, flags|os.O_CLOEXEC, mode) as int;
    if (fd < 0 || flags & os.O_CLOEXEC) {
        syscall.syscall3(syscall.sys_fcntl, fd, os.F_SETFD, os.FD_CLOEXEC);
    }
    return fd;
}

fn remove(path: string) -> int {
    r := syscall.syscall3(syscall.sys_unlinkat, AT_FDCWD, path.bytes, 0) as int;
    if (r == -syscall.EISDIR) {
        r = syscall.syscall3(syscall.sys_unlinkat, AT_FDCWD, path.bytes, AT_REMOVEDIR) as int;
    }
    return r;
}

fn write(fd:int, s:string) {
    syscall.write(fd, s, s.length);
}

fn close(fd:int) {
    syscall.syscall1(syscall.sys_close, fd);
}
