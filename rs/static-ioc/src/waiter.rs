//! 服务员

use crate::chef::Chef;

/// 服务员特性
pub trait Waiter {
	fn serve(&self) -> &str;
}

#[dep_inj_target::dep_inj_target]
pub struct Tom;

impl<Deps> Tom<Deps>
where Deps: Chef
{
	pub fn serve(&self) -> &str {
		//  使用 deref 机制自动使用 Trait `Chef` 下的方法
		self.prj_ref().cook()
		// Or `(self.prj_ref() as &dyn Chef).cook()`
	}
}
