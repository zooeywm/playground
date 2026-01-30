use std::{io, pin::Pin, sync::Arc, task::{Context, Poll}};

use compio::fs::File;

#[allow(dead_code)]
#[derive(Debug)]
pub struct User {
	id: String,
}

pub trait UserServiceWithCompioStaticFuture {
	async fn get_user(&self, id: &str) -> Option<User>;
	/// get_user 的 desugar 版本
	fn get_user_async_manual<'a>(&'a self, id: &'a str) -> impl Future<Output = Option<User>> + use<'a, Self>;
}

struct GetUserFuture<'a> {
	id:    &'a str,
	state: State<'a>,
}

enum State<'a> {
	Start,
	Opening(Pin<Box<dyn Future<Output = io::Result<File>> + 'a>>),
	Reading(Pin<Box<dyn Future<Output = Option<User>> + 'a>>),
}

impl<'a> Future for GetUserFuture<'a> {
	type Output = Option<User>;

	fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
		loop {
			match &mut self.state {
				State::Start => {
					let fut = Box::pin(compio::fs::File::open("Cargo.toml"));
					self.state = State::Opening(fut);
				}

				State::Opening(fut) => {
					let file = match fut.as_mut().poll(cx) {
						Poll::Pending => return Poll::Pending,
						Poll::Ready(Ok(f)) => f,
						Poll::Ready(Err(e)) => panic!("{e:?}"),
					};

					let id = self.id.to_string();

					let fut = Box::pin(async move {
						let (_read, buffer) =
							compio::io::AsyncReadAtExt::read_to_end_at(&file, Vec::with_capacity(1024), 0).await.unwrap();

						let text = String::from_utf8(buffer).ok()?;
						println!("{text}");

						Some(User { id })
					});

					self.state = State::Reading(fut);
				}

				State::Reading(fut) => {
					return fut.as_mut().poll(cx);
				}
			}
		}
	}
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

pub async fn use_service_manual_compio_static(service: impl UserServiceWithCompioStaticFuture) {
	let user = service.get_user_async_manual("user-123").await;
	println!("use_service_manual_compio_static: {user:?}");
}

pub async fn use_service_compio_static(service: impl UserServiceWithCompioStaticFuture) {
	let user = service.get_user("user-123").await;
	println!("use_service_compio_static: {user:?}");
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

impl UserServiceWithCompioStaticFuture for DbUserService {
	async fn get_user(&self, id: &str) -> Option<User> {
		let file = File::open("Cargo.toml").await.unwrap();
		let (read, buffer) =
			compio::io::AsyncReadAtExt::read_to_end_at(&file, Vec::with_capacity(1024), 0).await.unwrap();
		assert_eq!(read, buffer.len());
		let buffer = String::from_utf8(buffer).unwrap();
		println!("{buffer}");
		Some(User { id: id.to_string() })
	}

	fn get_user_async_manual<'a>(&'a self, id: &'a str) -> impl Future<Output = Option<User>> + use<'a> {
		GetUserFuture { id, state: State::Start }
	}
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
