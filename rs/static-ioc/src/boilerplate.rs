use crate::{
    Restaurant,
    chef::{Chef, Jack},
    waiter::{Tom, Waiter},
};

// 为容器提供 Trait 实现的样板
impl Chef for Restaurant {
    fn cook(&self) -> &str {
        // 使用 dep-inj 宏生成的 inj_ref(),
        // 从 Container 获取 &Jack 进而调用 Jack 的 cook 方法
        Jack::inj_ref(self).cook()
    }
}

impl Waiter for Restaurant {
    fn serve(&self) -> &str {
        Tom::inj_ref(self).serve()
    }
}
