#include "MLX90393_cmds.h"

#include <stdint.h>
#include <string.h>


#define X_EXPAND_MLX90393_CMDS(NAME, CMD, TX_LEN, RX_LEN) {#NAME, CMD, TX_LEN, RX_LEN},
const mlx90393_cmds_t MLX90393_CMD_OBJECTS[] = {MLX90393_CMDS_TABLE(X_EXPAND_MLX90393_CMDS)};

mlx90393_cmds_t mlx90393_cmds_get(const MLX90393_CMDS cmd) { return MLX90393_CMD_OBJECTS[cmd]; }

static mlx90393_status_t _status_ret_cmds_request(const uint8_t dev_id, const MLX90393_CMDS cmd) {
	const mlx90393_cmds_t cmd_obj = mlx90393_cmds_get(cmd);

	const uint8_t buf_len = cmd_obj.tx_len + cmd_obj.rx_len;

	uint8_t tx_buf[buf_len];
	uint8_t rx_buf[buf_len];
	memset(tx_buf + cmd_obj.tx_len, 0, cmd_obj.rx_len);
	memset(rx_buf, 0, buf_len);
	tx_buf[0] = cmd_obj.cmd;

	spi_cmd_t cmd_req = {
		.dev_id = dev_id,
		.tx_data = tx_buf,
		.rx_data = rx_buf,
		.len = buf_len,
	};
	spi_tx_request(&cmd_req);

	mlx90393_status_t status = {.raw = cmd_req.rx_data[cmd_obj.tx_len]};
	return status;
}

mlx90393_status_t mlx90393_SB_request(const uint8_t dev_id) { return _status_ret_cmds_request(dev_id, MLX90393_SB); }

mlx90393_status_t mlx90393_SW_request(const uint8_t dev_id) { return _status_ret_cmds_request(dev_id, MLX90393_SW); }

mlx90393_status_t mlx90393_SM_request(const uint8_t dev_id) { return _status_ret_cmds_request(dev_id, MLX90393_SM); }

mlx90393_status_t mlx90393_RT_request(const uint8_t dev_id) { return _status_ret_cmds_request(dev_id, MLX90393_RT); }

mlx90393_data_t mlx90393_RM_request(const uint8_t dev_id) {
	const mlx90393_cmds_t cmd_obj = mlx90393_cmds_get(MLX90393_RM);

	const uint8_t buf_len = cmd_obj.tx_len + cmd_obj.rx_len;

	uint8_t tx_buf[buf_len];
	uint8_t rx_buf[buf_len];
	memset(tx_buf + cmd_obj.tx_len, 0, cmd_obj.rx_len);
	memset(rx_buf, 0, buf_len);
	tx_buf[0] = cmd_obj.cmd;

	spi_cmd_t cmd_req = {
		.dev_id = dev_id,
		.tx_data = tx_buf,
		.rx_data = rx_buf,
		.len = buf_len,
	};
	spi_tx_request(&cmd_req);

	mlx90393_data_t data;
	memcpy(data.raw, cmd_req.rx_data + cmd_obj.tx_len, cmd_obj.rx_len);

	return data;
}

mlx90393_reg_data_t mlx90393_RR_request(const uint8_t dev_id, const uint8_t reg) {
	const mlx90393_cmds_t cmd_obj = mlx90393_cmds_get(MLX90393_RR);

	const uint8_t buf_len = cmd_obj.tx_len + cmd_obj.rx_len;

	uint8_t tx_buf[buf_len];
	uint8_t rx_buf[buf_len];
	memset(tx_buf + cmd_obj.tx_len, 0, cmd_obj.rx_len);
	memset(rx_buf, 0, buf_len);
	tx_buf[0] = cmd_obj.cmd;
	tx_buf[1] = reg << 2;

	spi_cmd_t cmd_req = {
		.dev_id = dev_id,
		.tx_data = tx_buf,
		.rx_data = rx_buf,
		.len = buf_len,
	};
	spi_tx_request(&cmd_req);

	mlx90393_reg_data_t data;
	memcpy(data.raw, cmd_req.rx_data + cmd_obj.tx_len, cmd_obj.rx_len);

	return data;
}


mlx90393_status_t mlx90393_WR_request(const uint8_t dev_id, const uint8_t reg, const uint8_t data[2]) {
	const mlx90393_cmds_t cmd_obj = mlx90393_cmds_get(MLX90393_WR);

	const uint8_t buf_len = cmd_obj.tx_len + cmd_obj.rx_len;

	uint8_t tx_buf[buf_len];
	uint8_t rx_buf[buf_len];
	memset(tx_buf + cmd_obj.tx_len, 0, cmd_obj.rx_len);
	memset(rx_buf, 0, buf_len);
	tx_buf[0] = cmd_obj.cmd;
	tx_buf[1] = data[0];
	tx_buf[2] = data[1];
	tx_buf[3] = reg << 2;

	spi_cmd_t cmd_req = {
		.dev_id = dev_id,
		.tx_data = tx_buf,
		.rx_data = rx_buf,
		.len = buf_len,
	};
	spi_tx_request(&cmd_req);

	mlx90393_status_t status = {.raw = cmd_req.rx_data[cmd_obj.tx_len]};
	return status;
}

bool mlx90393_RM_data_is_valid(const mlx90393_status_t data) {
	// return !data.error;
	return (data.raw >> 4) == 0x2; // sm_mode, no error
}
