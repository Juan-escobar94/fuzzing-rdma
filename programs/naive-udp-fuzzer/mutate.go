package main

func flip_bytewise(data *[]byte, pos int) {
	if pos == 1 || pos == 5 || pos == 6 || pos == 7 {
		return
	}
	(*data)[pos] = (*data)[pos] ^ 0b11111111
}

func flip_single_bit(data *[]byte, pos int, bit_pos uint8) {
	(*data)[pos] = (*data)[pos] ^ (1 << bit_pos)
}

func flip_bits(data *[]byte, pos int, bits uint8, bit_pos_start uint8) {
	if pos == 1 {
		return
	}
	if pos == 5 || pos == 6 || pos == 7 {
		return
	}
	switch bits {
	case 1:
		(*data)[pos] = (*data)[pos] ^ (0b1 << bit_pos_start)
	case 2:
		(*data)[pos] = (*data)[pos] ^ (0b11 << bit_pos_start)
	case 3:
		(*data)[pos] = (*data)[pos] ^ (0b111 << bit_pos_start)
	case 4:
		(*data)[pos] = (*data)[pos] ^ (0b1111 << bit_pos_start)
	}
}

func slice_cases(data1 []byte, data2 []byte, pos int) []byte {
	sliced := append(data1[:pos], data2[pos:]...)
	return sliced
}
