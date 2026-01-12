struct ListNode {
  value: i32,
  next: Option<Box<ListNode>>,
}

impl ListNode {
  #[inline]
  fn new(value: i32) -> Self {
    Self { value, next: None }
  }
}

impl From<ListNode> for Vec<i32> {
  fn from(val: ListNode) -> Self {
    let mut vec = Vec::new();
    let mut current = Some(Box::new(val));
    while let Some(node) = current {
      vec.push(node.value);
      current = node.next;
    }
    vec
  }
}

impl From<Vec<i32>> for ListNode {
  fn from(vec: Vec<i32>) -> Self {
    let mut dummy_head = ListNode::new(0);
    let mut current = &mut dummy_head;
    for &value in vec.iter() {
      current.next = Some(Box::new(ListNode::new(value)));
      current = current.next.as_mut().unwrap();
    }
    *dummy_head.next.unwrap()
  }
}

fn _add_two_numbers(
  mut list1: Option<Box<ListNode>>,
  mut list2: Option<Box<ListNode>>,
) -> Vec<i32> {
  let mut dummy_head = ListNode::new(0);
  let mut current = &mut dummy_head;
  let mut carry = 0;

  while list1.is_some() || list2.is_some() || carry > 0 {
    let val1 = list1.as_ref().map_or(0, |node| node.value);
    let val2 = list2.as_ref().map_or(0, |node| node.value);

    let sum = val1 + val2 + carry;
    carry = sum / 10;
    current.next = Some(Box::new(ListNode::new(sum % 10)));
    current = current.next.as_mut().unwrap();

    list1 = list1.and_then(|node| node.next);
    list2 = list2.and_then(|node| node.next);
  }

  dummy_head.next.map_or(vec![], |x| (*x).into())
}

#[test]
fn test() {
  assert_eq!(
    _add_two_numbers(
      Some(Box::new(ListNode::from(vec![2, 4, 3]))),
      Some(Box::new(ListNode::from(vec![5, 6, 4])))
    ),
    vec![7, 0, 8]
  );

  assert_eq!(
    _add_two_numbers(
      Some(Box::new(ListNode::from(vec![0]))),
      Some(Box::new(ListNode::from(vec![0])))
    ),
    vec![0]
  );

  assert_eq!(
    _add_two_numbers(
      Some(Box::new(ListNode::from(vec![9, 9, 9, 9, 9, 9, 9]))),
      Some(Box::new(ListNode::from(vec![9, 9, 9, 9])))
    ),
    vec![8, 9, 9, 9, 0, 0, 0, 1]
  );
}
