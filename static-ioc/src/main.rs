use crate::waiter::Waiter;

mod boilerplate;
mod chef;
mod waiter;

use crate::chef::JackState;

#[derive(derive_more::AsRef)]
pub struct Restaurant {
    #[as_ref]
    jack: JackState,
}

impl Restaurant {
    pub fn new() -> Self {
        let jack = JackState::new("麻婆豆腐".to_string());
        Self { jack }
    }
}

impl Default for Restaurant {
    fn default() -> Self {
        Self::new()
    }
}

fn main() {
    let container = Restaurant::new();
    // 容器调用 `serve()` 时，相当于调用 `boilerplate` 中我们实现的 `Tom`
    // 通过 `inj_ref` 注入的引用实现的 `Waiter` 的 `serve` 方法，
    // 因为`Tom` 在 impl `Waiter` 特性的时候通过 `Deps` 依赖了 `Chef` 这个特性,
    // 而我们在 `boilerplate` 中为容器实现了这个特性(通过 `Jack` `inj_ref` 注入的引用来调用 cook())
    // 所以 `serve()` 就获得了 `cook()` 的值，并返回了！
    let dish = container.serve();
    println!("{dish}");
}
