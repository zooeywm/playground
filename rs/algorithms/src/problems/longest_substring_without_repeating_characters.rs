fn _length_of_longest_substring(s: &str) -> i32 {
  let mut char_last_index_map = std::collections::HashMap::new();
  let mut max_length = 0;
  let mut start = 0;

  for (i, c) in s.chars().enumerate() {
    if let Some(&last_index) = char_last_index_map.get(&c) {
      start = start.max(last_index + 1);
    }
    char_last_index_map.insert(c, i);
    max_length = max_length.max(i - start + 1);
  }

  max_length as i32
}

#[test]
fn test() {
  assert_eq!(_length_of_longest_substring("abcabcbb"), 3);
  assert_eq!(_length_of_longest_substring("bbbbb"), 1);
  assert_eq!(_length_of_longest_substring("pwwkew"), 3);
}
