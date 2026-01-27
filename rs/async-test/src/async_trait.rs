use std::{pin::Pin, sync::Arc};

#[allow(dead_code)]
#[derive(Debug)]
pub struct User {
	id: String,
}

pub trait UserServiceWithStaticFuture {
	async fn get_user(&self, id: &str) -> Option<User>;
}

pub trait UserServiceWithDynamicSendFuture {
	fn get_user<'service, 'id, 'future>(
		&'service self,
		id: &'id str,
	) -> Pin<Box<dyn Future<Output = Option<User>> + Send + 'future>>
	where
		'service: 'future,
		'id: 'future;
}

pub trait UserServiceWithDynamicFuture {
	fn get_user<'service, 'id, 'future>(
		&'service self,
		id: &'id str,
	) -> Pin<Box<dyn Future<Output = Option<User>> + 'future>>
	where
		'service: 'future,
		'id: 'future;
}

pub async fn use_service_static(service: impl UserServiceWithStaticFuture) {
	let user = service.get_user("user-123").await;
	println!("use_service_static: {user:?}");
}

pub async fn use_service_dynamic_compio(service: Box<dyn UserServiceWithDynamicFuture>) {
	compio::runtime::spawn(async move {
		let user = service.get_user("user-123").await;
		println!("use_service_dynamic_compio: {user:?}");
	})
	.await
	.unwrap();
}

pub async fn use_service_dynamic_tokio(service: Arc<dyn UserServiceWithDynamicSendFuture + Send + Sync>) {
	tokio::spawn(async move {
		let user = service.get_user("user-123").await;
		println!("use_service_dynamic_tokio: {user:?}");
	});
}

pub struct DbUserService;

impl UserServiceWithStaticFuture for DbUserService {
	async fn get_user(&self, id: &str) -> Option<User> { Some(User { id: id.to_string() }) }
}

impl UserServiceWithDynamicFuture for DbUserService {
	fn get_user<'service, 'id, 'future>(
		&'service self,
		id: &'id str,
	) -> Pin<Box<dyn Future<Output = Option<User>> + 'future>>
	where
		'service: 'future,
		'id: 'future,
	{
		Box::pin(async move { Some(User { id: id.to_string() }) })
	}
}

impl UserServiceWithDynamicSendFuture for DbUserService {
	fn get_user<'service, 'id, 'future>(
		&'service self,
		id: &'id str,
	) -> Pin<Box<dyn Future<Output = Option<User>> + Send + 'future>>
	where
		'service: 'future,
		'id: 'future,
	{
		Box::pin(async move { Some(User { id: id.to_string() }) })
	}
}
