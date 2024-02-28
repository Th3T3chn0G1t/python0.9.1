# TestCSplit

import stdwin

from Buttons import PushButton
from WindowParent import WindowParent


def main(n):
    from CSplit import CSplit

    the_window = WindowParent().create('TestCSplit', (0, 0))
    the_csplit = CSplit().create(the_window)

    for i in range(n):
        the_child = PushButton().define(the_csplit)
        the_child.settext(`(i + n - 1) % n + 1`)

    the_window.realize()

    while 1:
        the_event = stdwin.getevent()
        if the_event[0] = WE_CLOSE: break
        the_window.dispatch(the_event)
    the_window.destroy()


main(12)
