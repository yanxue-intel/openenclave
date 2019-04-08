// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <openenclave/corelibc/dirent.h>
#include <openenclave/corelibc/errno.h>
#include <openenclave/corelibc/fcntl.h>
#include <openenclave/corelibc/setjmp.h>
#include <openenclave/corelibc/stdarg.h>
#include <openenclave/corelibc/stdio.h>
#include <openenclave/corelibc/stdlib.h>
#include <openenclave/corelibc/string.h>
#include <openenclave/corelibc/sys/poll.h>
#include <openenclave/corelibc/sys/select.h>
#include <openenclave/corelibc/sys/socket.h>
#include <openenclave/corelibc/sys/stat.h>
#include <openenclave/corelibc/sys/syscall.h>
#include <openenclave/corelibc/sys/uio.h>
#include <openenclave/corelibc/sys/utsname.h>
#include <openenclave/corelibc/unistd.h>
#include <openenclave/internal/device.h>
#include <openenclave/internal/epoll.h>
#include <openenclave/internal/eventfd.h>
#include <openenclave/internal/print.h>
#include <openenclave/internal/uid.h>

typedef int (*ioctl_proc)(
    int fd,
    unsigned long request,
    long arg1,
    long arg2,
    long arg3,
    long arg4);

static long _syscall(
    long num,
    long arg1,
    long arg2,
    long arg3,
    long arg4,
    long arg5,
    long arg6)
{
    long ret = -1;
    oe_errno = 0;

    /* Handle the software system call. */
    switch (num)
    {
        case OE_SYS_creat:
        {
            const char* pathname = (const char*)arg1;
            mode_t mode = (mode_t)arg2;
            int flags = (OE_O_CREAT | OE_O_WRONLY | OE_O_TRUNC);

            ret = oe_open(pathname, flags, mode);

            if (oe_errno == ENOENT)
            {
                /* Not handled. Let caller dispatch this syscall. */
                oe_errno = ENOSYS;
                goto done;
            }

            goto done;
        }
        case OE_SYS_open:
        {
            const char* pathname = (const char*)arg1;
            int flags = (int)arg2;
            uint32_t mode = (uint32_t)arg3;

            ret = oe_open(pathname, flags, mode);

            if (ret < 0 && oe_errno == ENOENT)
                goto done;

            goto done;
        }
        case OE_SYS_lseek:
        {
            int fd = (int)arg1;
            ssize_t off = (ssize_t)arg2;
            int whence = (int)arg3;

            ret = oe_lseek(fd, off, whence);
            goto done;
        }
        case OE_SYS_readv:
        {
            int fd = (int)arg1;
            const struct oe_iovec* iov = (const struct oe_iovec*)arg2;
            int iovcnt = (int)arg3;

            ret = oe_readv(fd, iov, iovcnt);
            goto done;
        }
        case OE_SYS_writev:
        {
            int fd = (int)arg1;
            const struct oe_iovec* iov = (const struct oe_iovec*)arg2;
            int iovcnt = (int)arg3;

            ret = oe_writev(fd, iov, iovcnt);
            goto done;
        }
        case OE_SYS_read:
        {
            int fd = (int)arg1;
            void* buf = (void*)arg2;
            size_t count = (size_t)arg3;

            ret = oe_read(fd, buf, count);
            goto done;
        }
        case OE_SYS_write:
        {
            int fd = (int)arg1;
            const void* buf = (void*)arg2;
            size_t count = (size_t)arg3;

            ret = oe_write(fd, buf, count);
            goto done;
        }
        case OE_SYS_close:
        {
            int fd = (int)arg1;

            ret = oe_close(fd);
            goto done;
        }
        case OE_SYS_dup:
        {
            int fd = (int)arg1;

            ret = oe_dup(fd);
            goto done;
        }
        case OE_SYS_dup2:
        {
            int fd = (int)arg1;
            int newfd = (int)arg2;

            ret = oe_dup2(fd, newfd);
            goto done;
        }
        case OE_SYS_stat:
        {
            const char* pathname = (const char*)arg1;
            struct oe_stat* buf_out = (struct oe_stat*)arg2;
            struct oe_stat buf;

            ret = oe_stat(pathname, &buf);
            memcpy(buf_out, &buf, sizeof(struct oe_stat));
            goto done;
        }
        case OE_SYS_link:
        {
            const char* oldpath = (const char*)arg1;
            const char* newpath = (const char*)arg2;

            ret = oe_link(oldpath, newpath);
            goto done;
        }
        case OE_SYS_unlink:
        {
            const char* pathname = (const char*)arg1;

            ret = oe_unlink(pathname);
            goto done;
        }
        case OE_SYS_rename:
        {
            const char* oldpath = (const char*)arg1;
            const char* newpath = (const char*)arg2;

            ret = oe_rename(oldpath, newpath);
            goto done;
        }
        case OE_SYS_truncate:
        {
            const char* path = (const char*)arg1;
            ssize_t length = (ssize_t)arg2;

            ret = oe_truncate(path, length);
            goto done;
        }
        case OE_SYS_mkdir:
        {
            const char* pathname = (const char*)arg1;
            uint32_t mode = (uint32_t)arg2;

            ret = oe_mkdir(pathname, mode);
            goto done;
        }
        case OE_SYS_rmdir:
        {
            const char* pathname = (const char*)arg1;
            ret = oe_rmdir(pathname);
            goto done;
        }
        case OE_SYS_access:
        {
            const char* pathname = (const char*)arg1;
            int mode = (int)arg2;

            ret = oe_access(pathname, mode);
            goto done;
        }
        case OE_SYS_getdents:
        case OE_SYS_getdents64:
        {
            unsigned int fd = (unsigned int)arg1;
            struct oe_dirent* ent = (struct oe_dirent*)arg2;
            unsigned int count = (unsigned int)arg3;
            ret = oe_getdents(fd, ent, count);
            goto done;
        }
        case OE_SYS_ioctl:
        {
            int fd = (int)arg1;
            unsigned long request = (unsigned long)arg2;
            long p1 = arg3;
            long p2 = arg4;
            long p3 = arg5;
            long p4 = arg6;

            ret = ((ioctl_proc)oe_ioctl)(fd, request, p1, p2, p3, p4);
            goto done;
        }
        case OE_SYS_fcntl:
        {
            int fd = (int)arg1;
            int cmd = (int)arg2;
            int arg = (int)arg3;
            ret = oe_fcntl(fd, cmd, arg);
            goto done;
        }
        case OE_SYS_mount:
        {
            const char* source = (const char*)arg1;
            const char* target = (const char*)arg2;
            const char* fstype = (const char*)arg3;
            unsigned long flags = (unsigned long)arg4;
            void* data = (void*)arg5;

            ret = oe_mount(source, target, fstype, flags, data);
            goto done;
        }
        case OE_SYS_umount2:
        {
            const char* target = (const char*)arg1;
            int flags = (int)arg2;

            (void)flags;

            ret = oe_umount(target);
            goto done;
        }
        case OE_SYS_getcwd:
        {
            char* buf = (char*)arg1;
            size_t size = (size_t)arg2;

            if (!oe_getcwd(buf, size))
            {
                ret = -1;
            }
            else
            {
                ret = (long)size;
            }

            goto done;
        }
        case OE_SYS_chdir:
        {
            char* path = (char*)arg1;

            ret = oe_chdir(path);
            goto done;
        }
        case OE_SYS_socket:
        {
            int domain = (int)arg1;
            int type = (int)arg2;
            int protocol = (int)arg3;
            ret = oe_socket(domain, type, protocol);
            goto done;
        }
        case OE_SYS_connect:
        {
            int sd = (int)arg1;
            const struct oe_sockaddr* addr = (const struct oe_sockaddr*)arg2;
            socklen_t addrlen = (socklen_t)arg3;
            ret = oe_connect(sd, addr, addrlen);
            goto done;
        }
        case OE_SYS_setsockopt:
        {
            int sockfd = (int)arg1;
            int level = (int)arg2;
            int optname = (int)arg3;
            void* optval = (void*)arg4;
            socklen_t optlen = (socklen_t)arg5;
            ret = oe_setsockopt(sockfd, level, optname, optval, optlen);
            goto done;
        }
        case OE_SYS_getsockopt:
        {
            int sockfd = (int)arg1;
            int level = (int)arg2;
            int optname = (int)arg3;
            void* optval = (void*)arg4;
            socklen_t* optlen = (socklen_t*)arg5;
            ret = oe_getsockopt(sockfd, level, optname, optval, optlen);
            goto done;
        }
        case OE_SYS_getpeername:
        {
            int sockfd = (int)arg1;
            struct sockaddr* addr = (struct sockaddr*)arg2;
            socklen_t* addrlen = (socklen_t*)arg3;
            ret = oe_getpeername(sockfd, (struct oe_sockaddr*)addr, addrlen);
            goto done;
        }
        case OE_SYS_getsockname:
        {
            int sockfd = (int)arg1;
            struct sockaddr* addr = (struct sockaddr*)arg2;
            socklen_t* addrlen = (socklen_t*)arg3;
            ret = oe_getsockname(sockfd, (struct oe_sockaddr*)addr, addrlen);
            goto done;
        }
        case OE_SYS_bind:
        {
            int sockfd = (int)arg1;
            struct oe_sockaddr* addr = (struct oe_sockaddr*)arg2;
            socklen_t addrlen = (socklen_t)arg3;
            ret = oe_bind(sockfd, addr, addrlen);
            goto done;
        }
        case OE_SYS_listen:
        {
            int sockfd = (int)arg1;
            int backlog = (int)arg2;
            ret = oe_listen(sockfd, backlog);
            goto done;
        }
        case OE_SYS_accept:
        {
            int sockfd = (int)arg1;
            struct oe_sockaddr* addr = (struct oe_sockaddr*)arg2;
            socklen_t* addrlen = (socklen_t*)arg3;
            ret = oe_accept(sockfd, addr, addrlen);
            goto done;
        }
        case OE_SYS_sendto:
        {
            int sockfd = (int)arg1;
            const void* buf = (void*)arg2;
            size_t len = (size_t)arg3;
            int flags = (int)arg4;
            const struct oe_sockaddr* dest_add =
                (const struct oe_sockaddr*)arg5;
            socklen_t addrlen = (socklen_t)arg6;

            ret = oe_sendto(sockfd, buf, len, flags, dest_add, addrlen);
            goto done;
        }
        case OE_SYS_recvfrom:
        {
            int sockfd = (int)arg1;
            void* buf = (void*)arg2;
            size_t len = (size_t)arg3;
            int flags = (int)arg4;
            const struct oe_sockaddr* dest_add =
                (const struct oe_sockaddr*)arg5;
            socklen_t* addrlen = (socklen_t*)arg6;

            ret = oe_recvfrom(sockfd, buf, len, flags, dest_add, addrlen);
            goto done;
        }
        case OE_SYS_sendmsg:
        {
            int sockfd = (int)arg1;
            struct msghdr* buf = (struct msghdr*)arg2;
            int flags = (int)arg3;

            ret = oe_sendmsg(sockfd, (struct oe_msghdr*)buf, flags);
            goto done;
        }
        case OE_SYS_recvmsg:
        {
            int sockfd = (int)arg1;
            struct msghdr* buf = (struct msghdr*)arg2;
            int flags = (int)arg3;

            ret = oe_recvmsg(sockfd, (struct oe_msghdr*)buf, flags);
            goto done;
        }
        case OE_SYS_socketpair:
        {
            int domain = (int)arg1;
            int type = (int)arg2;
            int protocol = (int)arg3;
            int* sv = (int*)arg4;

            ret = oe_socketpair(domain, type, protocol, sv);
            goto done;
        }
        case OE_SYS_shutdown:
        {
            int sockfd = (int)arg1;
            int how = (int)arg2;
            ret = oe_shutdown(sockfd, how);
            goto done;
        }
        case OE_SYS_uname:
        {
            struct oe_utsname* buf = (struct oe_utsname*)arg1;
            ret = oe_uname(buf);
            goto done;
        }
        case OE_SYS_select:
        {
            int nfds = (int)arg1;
            oe_fd_set* readfds = (oe_fd_set*)arg2;
            oe_fd_set* writefds = (oe_fd_set*)arg3;
            oe_fd_set* exceptfds = (oe_fd_set*)arg4;
            struct oe_timeval* tmo = (struct oe_timeval*)arg5;
            ret = oe_select(nfds, readfds, writefds, exceptfds, tmo);
            goto done;
        }
        case OE_SYS_poll:
        {
            struct oe_pollfd* fds = (struct oe_pollfd*)arg1;
            nfds_t nfds = (nfds_t)arg2;
            int millis = (int)arg3;
            ret = oe_poll(fds, nfds, millis);
            goto done;
        }
        case OE_SYS_epoll_create:
        {
            int size = (int)arg1;
            ret = oe_epoll_create(size);
            goto done;
        }
        case OE_SYS_epoll_create1:
        {
            int flags = (int)arg1;
            ret = oe_epoll_create1(flags);
            goto done;
        }
        case OE_SYS_epoll_wait:
        {
            int epfd = (int)arg1;
            struct oe_epoll_event* events = (struct oe_epoll_event*)arg2;
            int maxevents = (int)arg3;
            int timeout = (int)arg4;
            ret = oe_epoll_wait(epfd, events, maxevents, timeout);
            goto done;
        }
        case OE_SYS_epoll_pwait:
        {
            int epfd = (int)arg1;
            struct oe_epoll_event* events = (struct oe_epoll_event*)arg2;
            int maxevents = (int)arg3;
            int timeout = (int)arg4;
#if defined(SUPPORT_ENCLAVE_SIGNALS)
            const sigset_t* sigmask = (const sigset_t*)arg5;
            ret = oe_epoll_pwait(epfd, events, maxevents, timeout, sigmask);
#else
            ret = oe_epoll_wait(epfd, events, maxevents, timeout);
#endif
            goto done;
        }
        case OE_SYS_epoll_wait_old:
        {
            int epfd = (int)arg1;
            struct oe_epoll_event* events = (struct oe_epoll_event*)arg2;
            int maxevents = (int)arg3;
            int timeout = (int)arg4;
            ret = oe_epoll_wait(epfd, events, maxevents, timeout);
            goto done;
        }
        case OE_SYS_epoll_ctl:
        {
            int epfd = (int)arg1;
            int op = (int)arg2;
            int fd = (int)arg3;
            struct oe_epoll_event* event = (struct oe_epoll_event*)arg4;
            ret = oe_epoll_ctl(epfd, op, fd, event);
            goto done;
        }
        case OE_SYS_epoll_ctl_old:
        {
            int epfd = (int)arg1;
            int op = (int)arg2;
            int fd = (int)arg3;
            struct oe_epoll_event* event = (struct oe_epoll_event*)arg4;
            ret = oe_epoll_ctl(epfd, op, fd, event);
            goto done;
        }
        case OE_SYS_eventfd:
        case OE_SYS_eventfd2:
        {
            unsigned int initval = (unsigned int)arg1;
            int flags = (int)arg2;
            ret = oe_eventfd(initval, flags);
            goto done;
        }
        case OE_SYS_exit_group:
        {
            ret = 0;
            goto done;
        }
        case OE_SYS_exit:
        {
            int status = (int)arg1;
            oe_exit(status);
        }
        case OE_SYS_getpid:
        {
            ret = (long)oe_getpid();
            goto done;
        }
        case OE_SYS_getuid:
        {
            ret = (long)oe_getuid();
            goto done;
        }
        case OE_SYS_geteuid:
        {
            ret = (long)oe_geteuid();
            goto done;
        }
        case OE_SYS_getppid:
        {
            ret = (long)oe_getppid();
            goto done;
        }
        case OE_SYS_getpgrp:
        {
            ret = (long)oe_getpgrp();
            goto done;
        }
            // case OE_SYS_getegid:
        case OE_SYS_rt_sigaction:
        {
            int signum = (int)arg1;
            struct sigaction* act = (struct sigaction*)arg2;
            struct sigaction* oact = (struct sigaction*)arg3;

            ret = (long)oe_signal(signum, act, oact);
            goto done;
        }
        case OE_SYS_kill:
        {
            int pid = (int)arg1;
            int signum = (int)arg2;

            ret = (long)oe_kill(pid, signum);
            goto done;
        }

        default:
        {
            oe_errno = ENOSYS;
            goto done;
        }
    }

    /* Unreachable */
done:
    return ret;
}

long oe_syscall(long number, ...)
{
    long ret;

    oe_va_list ap;
    oe_va_start(ap, number);
    long arg1 = oe_va_arg(ap, long);
    long arg2 = oe_va_arg(ap, long);
    long arg3 = oe_va_arg(ap, long);
    long arg4 = oe_va_arg(ap, long);
    long arg5 = oe_va_arg(ap, long);
    long arg6 = oe_va_arg(ap, long);
    ret = _syscall(number, arg1, arg2, arg3, arg4, arg5, arg6);
    oe_va_end(ap);

    return ret;
}
