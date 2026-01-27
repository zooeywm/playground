struct Matrix<const ROW: usize, const COLUMN: usize> {
	data: [[f32; ROW]; COLUMN],
}

impl<const ROW: usize, const COLUMN: usize> Matrix<ROW, COLUMN> {
	fn new() -> Self { Self { data: [[0.; ROW]; COLUMN] } }
}

fn main() {
	let mut x = Matrix::<2, 3>::new();

	for c in 0..3 {
		for r in 0..2 {
			x.data[c][r] = (c * 10 + r) as f32;
		}
	}

	println!("Matrix raw memory layout:");
	for c in 0..3 {
		println!("{:?}", x.data[c]);
	}

	assert_eq!(x.data[0][0], 0.0);
	assert_eq!(x.data[1][0], 10.0);
	assert_eq!(x.data[2][1], 21.0);

	println!("OK");
}
