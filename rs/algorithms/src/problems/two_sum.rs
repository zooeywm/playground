fn _two_sum(nums: &[i32], target: i32) -> Vec<usize> {
  let mut map = std::collections::HashMap::new();
  for (i, &num) in nums.iter().enumerate() {
    if let Some(&j) = map.get(&(target - num)) {
      return vec![j, i];
    }
    map.insert(num, i);
  }
  vec![]
}

#[test]
fn test() {
  assert_eq!(&[0, 1][..], &_two_sum(&[2, 7, 11, 15], 9));
  assert_eq!(&[1, 2][..], &_two_sum(&[3, 2, 4], 6));
  assert_eq!(&[0, 1][..], &_two_sum(&[3, 3], 6));
}
