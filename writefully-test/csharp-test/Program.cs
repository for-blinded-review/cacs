class WritefullyTest
{
    static long GetTimeStamp()
    {
        return Convert.ToInt64(
            (DateTime.UtcNow - new DateTime(1970, 1, 1, 0, 0, 0, 0))
            .TotalNanoseconds);
    }

    static async Task wait_for_ready()
    {
        await Task.Yield();
    }

    static async Task<long> io_some(long count)
    {
        await wait_for_ready();
        return 8;
    }

    static async Task<long> io_fully(long count)
    {
        long size = 0;
        while (count > 0)
        {
            long s = await io_some(count);
            if (s < 0)
                return s;
            else if (s == 0)
                return size;
            count -= s;
            size += s;
        }
        return size;
    }

    static void Main(string[] args)
    {
        ThreadPool.SetMinThreads(1, 1);
        ThreadPool.SetMaxThreads(1, 1);
        // Display the number of command line arguments.
        long start = 0;
        long end = 0;
        for (int round = 0; round < 3; round++)
        {
            long total = 0;
            long ma = 0;
            long mi = long.MaxValue;

            for (int i = 0; i < 10; i++)
            {
                
                List<Task<long>> lt = new List<Task<long>>();

                start = GetTimeStamp();
                for (int _ = 0; _ < 10; _++)
                {
                    lt.Add(io_fully(10 * 1024 * 1024));
                }
                Task.WaitAll(lt.ToArray());
                end = GetTimeStamp();
                ma = (end - start) > ma ? end - start : ma;
                mi = (end - start) < mi ? end - start : mi;
                total += (end - start);
                Console.WriteLine("spent {0} ns",
                        end - start);

            }
            Console.WriteLine("Average spent {0} ns",
                    (total - ma - mi) / 8);

        }
    }
}