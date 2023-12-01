use std::time::Instant;
use tokio::{io, task};

async fn ready_for_io() {
    tokio::task::yield_now().await
}

async fn io_some(_: i64) -> i64 {
    ready_for_io().await;
    return 8;
}

async fn io_fully(mut count: i64) -> i64 {
    let mut size: i64 = 0;
    while count > 0 {
        let s = io_some(count).await;
        match s {
            s if s < 0 => return s,
            _ if s == 0 => return size,
            _ => (),
        }
        count -= s;
        size += s;
    }
    return size;
}

#[tokio::main(flavor = "current_thread")]
async fn main() -> io::Result<()> {
    for _i in 0..3 {
        let mut set = task::JoinSet::new();
        let start = Instant::now();
        for _ in 0..10 {
            set.spawn(async move { io_fully(10 * 1024 * 1024).await });
        }
        while let Some(_res) = set.join_next().await {}
        println!("runtime {} us", start.elapsed().as_micros());
    }
    Ok(())
}
