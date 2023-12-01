class WritefullyTest
{
    static IEnumerable<int> Hanoi(uint n, char f, char t, char a)
    {
        if (n == 0) yield break;
        foreach (int x in Hanoi(n - 1, f, a, t))
        {
            yield return 1;
        }
        yield return 1;
        foreach (int x in Hanoi(n - 1, a, t, f))
        {
            yield return 1;
        }
    }

    public delegate void YieldCallback(uint x);
    static void Hanoi_CB(uint n, char f, char t, char a, YieldCallback cb)
    {
        if (n == 0) return;
        Hanoi_CB(n - 1, f, a, t, cb);
        cb(1);
        Hanoi_CB(n - 1, a, t, f, cb);
    }

    static long GetTimeStamp()
    {
        return Convert.ToInt64(
            (DateTime.UtcNow - new DateTime(1970, 1, 1, 0, 0, 0, 0))
            .TotalNanoseconds);
    }


    static void Main(string[] args)
    {
        ThreadPool.SetMinThreads(1, 1);
        ThreadPool.SetMaxThreads(1, 1);
        // Display the number of command line arguments.
        uint ret = 0;
        long start = 0;
        long end = 0;
        for (int round = 0; round < 3; round++)
        {
            for (uint l = 1; l <= 20; l++)
            {
                for (uint _ = 0; _ < 10; _++)
                {
                    ret = 0;
                    foreach (int x in Hanoi(l, 'a', 'c', 'b'))
                        ret++;
                }
                long total = 0;
                long ma = 0;
                long mi = long.MaxValue;
                for (uint _ = 0; _ < 10; _++)
                {
                    ret = 0;
                    start = GetTimeStamp();
                    foreach (int x in Hanoi(l, 'a', 'c', 'b'))
                        ret++;
                    end = GetTimeStamp();
                    ma = (end - start) > ma ? end - start : ma;
                    mi = (end - start) < mi ? end - start : mi;
                    total += (end - start);
                }
                Console.WriteLine("level={0} ret={1} spent {2} ns", l, ret,
                        (total - ma - mi)/18);

            }


            for (uint l = 1; l <= 20; l++)
            {
                for (uint _ = 0; _ < 10; _++)
                {
                    ret = 0;
                    foreach (int x in Hanoi(l, 'a', 'c', 'b'))
                        ret++;
                }
                long total = 0;
                long ma = 0;
                long mi = long.MaxValue;
                for (uint _ = 0; _ < 10; _++)
                {
                    ret = 0;
                    start = GetTimeStamp();
                    foreach (int x in Hanoi(l, 'a', 'c', 'b'))
                        ret++;
                    end = GetTimeStamp();
                    ma = (end - start) > ma ? end - start : ma;
                    mi = (end - start) < mi ? end - start : mi;
                    total += (end - start);
                }
                Console.WriteLine("level={0} ret={1} spent {2} ns", l, ret,
                        (total - ma - mi)/18);

            }

            Console.WriteLine("Recursive");
            for (uint l = 1; l <= 20; l++)
            {
                for (uint _ = 0; _ < 10; _++)
                {
                    ret = 0;
                    Hanoi_CB(l, 'a', 'c', 'b', x=>ret+=x);
                }
                long total = 0;
                long ma = 0;
                long mi = long.MaxValue;
                for (uint _ = 0; _ < 20; _++)
                {
                    ret = 0;
                    start = GetTimeStamp();
                    Hanoi_CB(l, 'a', 'c', 'b', x=>ret+=x);
                    end = GetTimeStamp();
                    ma = (end - start) > ma ? end - start : ma;
                    mi = (end - start) < mi ? end - start : mi;
                    total += (end - start);
                }
                Console.WriteLine("level={0} ret={1} spent {2} ns", l, ret,
                        ((total - ma - mi) / 18));

            }

            for (uint l = 1; l <= 20; l++)
            {
                for (uint _ = 0; _ < 10; _++)
                {
                    ret = 0;
                    Hanoi_CB(l, 'a', 'c', 'b', x=>ret+=x);
                }
                long total = 0;
                long ma = 0;
                long mi = long.MaxValue;
                for (uint _ = 0; _ < 20; _++)
                {
                    ret = 0;
                    start = GetTimeStamp();
                    Hanoi_CB(l, 'a', 'c', 'b', x=>ret+=x);
                    end = GetTimeStamp();
                    ma = (end - start) > ma ? end - start : ma;
                    mi = (end - start) < mi ? end - start : mi;
                    total += (end - start);
                }
                Console.WriteLine("level={0} ret={1} spent {2} ns", l, ret,
                        ((total - ma - mi) / 18));

            }
        }
    }
}