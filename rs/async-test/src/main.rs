use std::sync::Arc;

use crate::async_trait::{DbUserService, use_service_compio_static, use_service_dynamic_compio, use_service_dynamic_tokio, use_service_manual_compio_static};

mod async_trait;

fn main() {
	compio::runtime::Runtime::new().unwrap().block_on(async {
		use_service_manual_compio_static(DbUserService).await;
		use_service_compio_static(DbUserService).await;
		use_service_dynamic_compio(Box::new(DbUserService)).await;
	});

	tokio::runtime::Builder::new_multi_thread().worker_threads(4).enable_all().build().unwrap().block_on(
		async {
			use_service_dynamic_tokio(Arc::new(DbUserService)).await;
		},
	);
}
