import timeit


def hanoi(n: int, f: str, t: str, a: str):
    if n == 0:
        return
    yield from hanoi(n-1, f, a, t)
    yield 1
    yield from hanoi(n-1, a, t, f)


def hanoi_rec(n: int, f: str, t: str, a: str, cb):
    if n == 0:
        return
    hanoi_rec(n-1, f, a, t, cb)
    cb(1)
    hanoi_rec(n-1, a, t, f, cb)


def test1(n: int):
    ret = 0
    for x in hanoi(n, 'a', 'c', 'b'):
        ret += x
    return ret


class Result:
    def __init__(self):
        self.ret = 0

    def callback(self, x: int):
        self.ret += x


def test2(n: int):
    ret = Result()
    hanoi_rec(n, 'a', 'c', 'b', ret.callback)
    return ret.ret


if __name__ == '__main__':
    for level in range(1, 21):
        print(
            "level={}, gen time= {} ns".format(
                level,
                timeit.timeit(lambda: test1(level), number=10) * 1E9 ))
    for level in range(1, 21):
        print(
            "level={}, rec time= {} ns".format(
                level,
                timeit.timeit(lambda: test2(level), number=10) * 1E9 ))
