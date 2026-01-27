use compio::{fs::File, io::AsyncReadAtExt};

#[compio::main]
async fn main() {
	let file = File::open("Cargo.toml").await.unwrap();
	let (read, buffer) = file.read_to_end_at(Vec::with_capacity(1024), 0).await.unwrap();
	assert_eq!(read, buffer.len());
	let buffer = String::from_utf8(buffer).unwrap();
	println!("{buffer}");
}
