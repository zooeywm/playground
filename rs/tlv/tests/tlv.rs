//! TLV encoded scheme information
//! For checking the use of messaging target, we divide Message into three
//! types: BiDirect, FromRemote, and ToRemote.
//!
//! So we can call read and write on `[TestMessage]`
//! Only can call read on `[FromTestMessage]`
//! Only can call write on `[ToTestMessage]`
//!
//! **WARN**: The fields are read or write by sort, so change them carefully.
//!
//! **NOTE**: For unknown reason, when add cfg-test derive `BinWrite` to
//! `[FromTestMessage]`, the rust-analyzer will report an error, but can build,
//! run, or test successfully. But do the similar thing to `[ToTestMessage]`
//! will not result in an rust-analyzer error. I've test it, but to avoid ra
//! reporting error, I commented the write test codes for `[FromTestMessage]`.

use binrw::{BinRead, BinResult, BinWrite};
use serde::{Deserialize, Serialize};

#[derive(BinRead, BinWrite)]
#[brw(little)]
#[cfg_attr(test, derive(Debug, Eq, PartialEq))]
pub struct TestMessage {
	pub r#type: TestMessageType,
	#[bw(write_with = custom_writer)]
	#[br(parse_with = custom_reader)]
	pub value:  TestMessageValue,
}

#[derive(BinRead)]
#[br(little)]
// #[cfg_attr(test, derive(BinWrite, Debug, Eq, PartialEq), bw(little))]
#[cfg_attr(test, derive(Debug, Eq, PartialEq))]
pub struct FromTestMessage {
	pub r#type: FromTestMessageType,
	#[br(parse_with = custom_reader)]
	// #[cfg_attr(test, bw(write_with = custom_writer))]
	pub value: TestMessageValue,
}

#[derive(BinWrite)]
#[bw(little)]
#[cfg_attr(test, derive(BinRead, Debug, Eq, PartialEq), br(little))]
pub struct ToTestMessage {
	pub r#type: ToTestMessageType,
	#[bw(write_with = custom_writer)]
	#[cfg_attr(test, br(parse_with = custom_reader))]
	pub value:  TestMessageValue,
}

#[derive(BinRead, BinWrite)]
#[brw(little, repr = u32)]
#[cfg_attr(test, derive(Debug, Eq, PartialEq))]
pub enum TestMessageType {
	HeartBeat = 0x1,
}

#[derive(BinRead)]
#[br(little, repr = u32)]
// #[cfg_attr(test, derive(BinWrite, Debug, Eq, PartialEq), bw(little, repr = u32))]
#[cfg_attr(test, derive(Debug, Eq, PartialEq))]
pub enum FromTestMessageType {
	Message1 = 5,
	Message2,
	Message3,
	Message4,
	Message5,
	Message6,
	Message7,
	Message8,
	Message9 = 17,
}

#[derive(BinWrite, BinRead, Debug, Eq, PartialEq)]
#[bw(little, repr = u32)]
#[br(little, repr = u32,)]
pub enum ToTestMessageType {
	Message1 = 2,
	Message2,
	Message3,
	Message4 = 16,
}

#[derive(Serialize, Deserialize)]
#[cfg_attr(test, derive(Debug, Eq, PartialEq))]
pub struct TestMessageValue {
	pub id:   String,
	#[serde(rename = "companyCode")]
	pub code: TestCode,
	pub data: Option<String>,
}

#[derive(Serialize, Deserialize)]
#[cfg_attr(test, derive(Debug, Eq, PartialEq))]
pub enum TestCode {
	TestInfo,
	#[serde(rename = "Company1")]
	Company1,
	#[serde(rename = "Company2")]
	Company2,
}
/// Custom writer for both length and value.
#[binrw::writer(writer, endian)]
fn custom_writer(value: &TestMessageValue) -> BinResult<()> {
	let pos = writer.stream_position()?;
	// Serialize
	let mut msg_value =
		serde_json::to_vec(value).map_err(|e| binrw::Error::Custom { pos, err: Box::new(e) })?;
	msg_value = format_send_msg_value(msg_value);
	// Get value length, and add 1(the length of '0x0d' byte).
	let length = msg_value.len() as u32;
	// Write value length first.
	length.write_options(writer, endian, ())?;
	// Then write value
	msg_value.write_options(writer, endian, ())
}

/// Custom reader for both length and value.
#[binrw::parser(reader, endian)]
fn custom_reader() -> BinResult<TestMessageValue> {
	// Read value length.
	let length = u32::read_options(reader, endian, ())?;
	let pos = reader.stream_position()?;
	let mut msg_value = vec![0; length as usize];
	// Read value.
	reader.read_exact(&mut msg_value)?;

	msg_value =
		format_receive_msg_value(msg_value).map_err(|e| binrw::Error::Custom { pos, err: Box::new(e) })?;
	// Deserialize
	serde_json::from_slice::<TestMessageValue>(&msg_value)
		.map_err(|e| binrw::Error::Custom { pos, err: Box::new(e) })
}

// Format send message value. Take ownership for performance, and we won't use
// original message value later.
fn format_send_msg_value(mut msg_value: Vec<u8>) -> Vec<u8> {
	// Refers to doc 2.5.2, replace these bytes
	msg_value = msg_value
		.into_iter()
		.flat_map(|v| match v {
			0x0e => vec![0x0e, 0x01],
			0x0d => vec![0x0e, 0x02],
			_ => vec![v],
		})
		.collect::<Vec<_>>();
	// Doc 2.5.1
	msg_value.push(0x0d);
	msg_value
}

// Format receive message value. Take ownership for performance, and we won't
// use original message value later.
fn format_receive_msg_value(mut msg_value: Vec<u8>) -> anyhow::Result<Vec<u8>> {
	if let Some(end_byte) = msg_value.pop()
		&& end_byte != 0x0d
	{
		anyhow::bail!("Error: Message do not end with 0x0d");
	};
	let mut result = Vec::new();
	let mut iter = msg_value.into_iter().peekable();
	while let Some(&v) = iter.peek() {
		iter.next();
		match v {
			0x0e_u8 => {
				if let Some(&next_v) = iter.peek() {
					if next_v == 0x01_u8 {
						result.push(0x0e); // Replace [0x0e, 0x01] with [0x0e]
						iter.next(); // Consume the next element (0x01)
						continue;
					}
					if next_v == 0x02_u8 {
						result.push(0x0d); // Replace [0x0e, 0x02] with [0x0d]
						iter.next(); // Consume the next element (0x02)
						continue;
					}
					result.push(v);
				}
			}
			_ => result.push(v),
		}
	}
	Ok(result)
}

#[cfg(test)]
mod tests {
	use std::io::Cursor;

	use super::*;

	#[test]
	fn serde_json_to_string() {
		let json =
			r#"{"id":"01466d6c-496c-4b9d-a34b-c0f1cfc47ca4","companyCode":"Company1","data":"extra-data"}"#;
		let message_value = serde_json::from_str::<TestMessageValue>(json).unwrap();
		assert_eq!(message_value, TestMessageValue {
			id:   "01466d6c-496c-4b9d-a34b-c0f1cfc47ca4".to_string(),
			code: TestCode::Company1,
			data: Some("extra-data".to_string()),
		});
		let json2 = serde_json::to_string(&message_value).unwrap();
		assert_eq!(json, json2);
	}

	#[test]
	fn binary_send() {
		let msg = ToTestMessage {
			r#type: ToTestMessageType::Message1,
			value:  TestMessageValue {
				id:   "01466d6c-496c-4b9d-a34b-c0f1cfc47ca4".to_string(),
				code: TestCode::Company1,
				data: Some("extra-data".to_string()),
			},
		};
		let mut writer = Cursor::new(Vec::new());
		msg.write(&mut writer).unwrap();
		let bytes = writer.into_inner();
		assert_eq!(bytes, vec![
			0x02, 0x00, 0x00, 0x00, 0x5b, 0x00, 0x00, 0x00, 0x7b, 0x22, 0x69, 0x64, 0x22, 0x3a, 0x22, 0x30, 0x31,
			0x34, 0x36, 0x36, 0x64, 0x36, 0x63, 0x2d, 0x34, 0x39, 0x36, 0x63, 0x2d, 0x34, 0x62, 0x39, 0x64, 0x2d,
			0x61, 0x33, 0x34, 0x62, 0x2d, 0x63, 0x30, 0x66, 0x31, 0x63, 0x66, 0x63, 0x34, 0x37, 0x63, 0x61, 0x34,
			0x22, 0x2c, 0x22, 0x63, 0x6f, 0x6d, 0x70, 0x61, 0x6e, 0x79, 0x43, 0x6f, 0x64, 0x65, 0x22, 0x3a, 0x22,
			0x43, 0x6f, 0x6d, 0x70, 0x61, 0x6e, 0x79, 0x31, 0x22, 0x2c, 0x22, 0x64, 0x61, 0x74, 0x61, 0x22, 0x3a,
			0x22, 0x65, 0x78, 0x74, 0x72, 0x61, 0x2d, 0x64, 0x61, 0x74, 0x61, 0x22, 0x7d, 0x0d,
		]);
		let mut cursor = Cursor::new(bytes);
		let msg_receive = ToTestMessage::read(&mut cursor).unwrap();
		assert_eq!(msg, msg_receive);
	}

	#[test]
	fn binary_receive() {
		let msg = FromTestMessage {
			r#type: FromTestMessageType::Message6,
			value:  TestMessageValue {
				id:   "af0a2097-8443-4545-93aa-c6aa5ae25b4a".to_string(),
				code: TestCode::Company2,
				data: None,
			},
		};
		// let mut writer = Cursor::new(Vec::new());
		// msg.write(&mut writer).unwrap();
		// let bytes = writer.into_inner();
		let b = vec![
			0x0a, 0x00, 0x00, 0x00, 0x53, 0x00, 0x00, 0x00, 0x7b, 0x22, 0x69, 0x64, 0x22, 0x3a, 0x22, 0x61, 0x66,
			0x30, 0x61, 0x32, 0x30, 0x39, 0x37, 0x2d, 0x38, 0x34, 0x34, 0x33, 0x2d, 0x34, 0x35, 0x34, 0x35, 0x2d,
			0x39, 0x33, 0x61, 0x61, 0x2d, 0x63, 0x36, 0x61, 0x61, 0x35, 0x61, 0x65, 0x32, 0x35, 0x62, 0x34, 0x61,
			0x22, 0x2c, 0x22, 0x63, 0x6f, 0x6d, 0x70, 0x61, 0x6e, 0x79, 0x43, 0x6f, 0x64, 0x65, 0x22, 0x3a, 0x22,
			0x43, 0x6f, 0x6d, 0x70, 0x61, 0x6e, 0x79, 0x32, 0x22, 0x2c, 0x22, 0x64, 0x61, 0x74, 0x61, 0x22, 0x3a,
			0x6e, 0x75, 0x6c, 0x6c, 0x7d, 0x0d,
		];
		// assert_eq!(
		//     bytes,
		//     b
		// );
		let mut cursor = Cursor::new(b);
		let msg_receive = FromTestMessage::read(&mut cursor).unwrap();
		assert_eq!(msg, msg_receive);
	}

	#[test]
	fn binary_send_and_receive() {
		let send_message = TestMessage {
			r#type: TestMessageType::HeartBeat,
			value:  TestMessageValue { id: "test-id".to_string(), code: TestCode::Company1, data: None },
		};

		// Write message to binary.
		let mut writer = Cursor::new(Vec::new());
		send_message.write(&mut writer).unwrap();

		// Read message from binary.
		let bytes = writer.into_inner();
		// println!("{bytes:?}");

		let mut cursor = Cursor::new(bytes);
		let receive_message = TestMessage::read(&mut cursor).unwrap();
		assert_eq!(send_message, receive_message);
	}

	#[test]
	fn send_message_value_format() {
		let mut message_value = vec![0x01, 0x02, 0x03, 0x04, 0x0d, 0xe, 0x0f, 0x10];
		message_value = format_send_msg_value(message_value);
		assert_eq!(message_value, vec![0x01, 0x02, 0x03, 0x04, 0x0e, 0x02, 0x0e, 0x01, 0x0f, 0x10, 0x0d])
	}

	#[test]
	fn receive_message_value_format() {
		let mut message_value = vec![0x05, 0x06, 0x0e, 0x02, 0xf0, 0x0d];
		message_value = format_receive_msg_value(message_value).unwrap();
		assert_eq!(message_value, vec![0x05, 0x06, 0x0d, 0xf0])
	}
}
