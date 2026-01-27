use std::sync::Arc;

use compio::{fs::File, io::AsyncReadAtExt};

use crate::async_trait::{DbUserService, use_service_dynamic_compio, use_service_dynamic_tokio, use_service_static};

mod async_trait;

fn main() {
	let compio_rt = compio::runtime::Runtime::new().expect("cannot create runtime");

	compio_rt.block_on(async move {
		let file = File::open("Cargo.toml").await.unwrap();
		let (read, buffer) = file.read_to_end_at(Vec::with_capacity(1024), 0).await.unwrap();
		assert_eq!(read, buffer.len());
		let buffer = String::from_utf8(buffer).unwrap();
		println!("{buffer}");
		use_service_static(DbUserService).await;
		use_service_dynamic_compio(Box::new(DbUserService)).await;
	});

	let tokio_rt = tokio::runtime::Builder::new_multi_thread()
		.worker_threads(4)
		.enable_all()
		.build()
		.expect("create tokio runtime");

	tokio_rt.block_on(async {
		use_service_static(DbUserService).await;
		use_service_dynamic_tokio(Arc::new(DbUserService)).await;
	});
}
