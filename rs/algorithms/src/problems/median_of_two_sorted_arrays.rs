fn _find_median_sorted_arrays(nums1: &[i32], nums2: &[i32]) -> f64 {
  let mut merged = Vec::with_capacity(nums1.len() + nums2.len());
  let (mut i, mut j) = (0, 0);

  while i < nums1.len() && j < nums2.len() {
    if nums1[i] < nums2[j] {
      merged.push(nums1[i]);
      i += 1;
    } else {
      merged.push(nums2[j]);
      j += 1;
    }
  }

  while i < nums1.len() {
    merged.push(nums1[i]);
    i += 1;
  }

  while j < nums2.len() {
    merged.push(nums2[j]);
    j += 1;
  }

  let mid = merged.len() / 2;
  if merged.len() % 2 == 0 {
    (merged[mid - 1] + merged[mid]) as f64 / 2.0
  } else {
    merged[mid] as f64
  }
}

#[test]
fn test() {
  assert_eq!(_find_median_sorted_arrays(&[1, 3], &[2]), 2.0);
  assert_eq!(_find_median_sorted_arrays(&[1, 2], &[3, 4]), 2.5);
  assert_eq!(_find_median_sorted_arrays(&[], &[1]), 1.0);
  assert_eq!(_find_median_sorted_arrays(&[2], &[]), 2.0);
  assert_eq!(_find_median_sorted_arrays(&[1, 2, 3], &[4, 5]), 3.0);
}
