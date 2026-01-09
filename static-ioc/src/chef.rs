//! 厨师

/// 厨师特性
pub trait Chef {
    fn cook(&self) -> &str;
}

#[derive(dep_inj::DepInj)]
#[target(Jack)]
pub struct JackState {
    skill: String,
}

impl JackState {
    pub fn new(skill: String) -> Self {
        Self { skill }
    }
}

impl<Deps> Chef for Jack<Deps>
where
    Deps: AsRef<JackState>,
{
    /// 我们假设厨师做饭不需要依赖其他特性，
    /// 意味着它不依赖任何其他结构体。
    /// 因此实际上无需为其派生 DepInj。
    /// 其实以下代码即可实现：
    /// ```rust
    /// pub struct Jack { skill: String }
    /// impl Chef for Jack {
    ///     fn cook(&self) {
    ///         println!("{}", self.skill);
    ///     }
    /// }
    /// ```
    /// 此处使用 DepInj 仅作示例。
    /// 若需访问 state 中的字段，必须显式指定
    fn cook(&self) -> &str {
        &self.skill
    }
}
