Currently the method context do not have direct access to the value symbols outside the object (type symbols are not restricted)

Therefore `@.` prefixes are required to access non-object value symbols.
For example:
```
function OSocketWatcher.AddSocket(asock : ONanoSocket, aobj_link : Object, ainev : FInEventHandler, outev : FOutEventHandler):
    if asock.socket == @.INVALID_SOCKET:
        raise ENanoNet('SocketWatcher.AddSocket: invalid socket handle.')
    endif

    asock.obj_link = aobj_link
    asock.in_event_handler = ainev
    asock.out_event_handler = outev

    var ev : SEpollEvent
    ev.events = 0
    if asock.in_event_handler != null:
        ev.events =OR= @.EPOLLIN
    endif
    if asock.out_event_handler != null:
        ev.events =OR= @.EPOLLOUT
    endif
    ev.data = pointer(asock)

    var r : int = @.libc_epoll_ctl(fd_epoll, @.EPOLL_CTL_ADD, asock.socket, &ev)
    if r < 0:
        if @.SocketError() == @.EEXIST:
            r = @.libc_epoll_ctl(fd_epoll, @.EPOLL_CTL_MOD, asock.socket, &ev)
        endif
        if r < 0:
            raise ENanoNet('SocketWatcher.AddSocket: Error adding socket to epoll.')
        endif
    endif
endfunc
```

The `use` statement is valid only in module root (already existing) and method bodies. In method bodies it has a different syntax so different parser.

A method-body `use` does not import modules and does not create namespaces.
It injects value symbols from already existing module namespaces into the **current block scope** for unqualified lookup.

`use *` injects the current module's effective merged root lookup (including the module own value symbols)

`use .` injects only the value symbols declared by the current module.

`use alias1, alias2` injects the value symbols selected for root-level merge by
the corresponding module-root `use` declaration. `only` / `exclude` are preserved.
A root `use mod --` has no selected merge symbols, so method-body `use mod`
has no effect and should warn.

Repeated method-use aliases should not have an effect. The compiler should check the already method-used scopes first.

After `use *`, later `use alias` or `use .` statements have no effect and
should warn. Repeated `use *` statements are accepted without warning.

`use *` cannot be combined with any other method-use item in the same statement.

Value symbol searching order in method body:
1. locals / params
2. object members
3. method-use injected symbols

`use *, aliasn` should be an error, no other aliases are allowed with `*`

`use ., aliasn` is allowed


Including `use *` allows to drop the `@.` prefixes:
```
function OSocketWatcher.AddSocket(asock : ONanoSocket, aobj_link : Object, ainev : FInEventHandler, outev : FOutEventHandler):
    use *
    if asock.socket == INVALID_SOCKET:
        raise ENanoNet('SocketWatcher.AddSocket: invalid socket handle.')
    endif

    asock.obj_link = aobj_link
    asock.in_event_handler = ainev
    asock.out_event_handler = outev

    var ev : SEpollEvent
    ev.events = 0
    if asock.in_event_handler != null:
        ev.events =OR= EPOLLIN
    endif
    if asock.out_event_handler != null:
        ev.events =OR= EPOLLOUT
    endif
    ev.data = pointer(asock)

    var r : int = libc_epoll_ctl(fd_epoll, EPOLL_CTL_ADD, asock.socket, &ev)
    if r < 0:
        if SocketError() == EEXIST:
            r = libc_epoll_ctl(fd_epoll, EPOLL_CTL_MOD, asock.socket, &ev)
        endif
        if r < 0:
            raise ENanoNet('SocketWatcher.AddSocket: Error adding socket to epoll.')
        endif
    endif
endfunc
```