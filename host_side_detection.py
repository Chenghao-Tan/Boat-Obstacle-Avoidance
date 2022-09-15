import numpy as np


def detection(config, INPUT_SHAPE, mask, depth):
    mask = np.array(mask).reshape(INPUT_SHAPE[1], INPUT_SHAPE[0])
    depth = depth.astype("float64")

    grid_num_h = config["GRID_NUM"][0]
    grid_num_w = config["GRID_NUM"][1]
    assert mask.shape[0] % grid_num_h == 0 and mask.shape[1] % grid_num_w == 0
    grid_height = mask.shape[0] // grid_num_h
    grid_width = mask.shape[1] // grid_num_w
    if isinstance(config["GRID_THRESHOLD"], float):
        assert config["GRID_THRESHOLD"] >= 0 and config["GRID_THRESHOLD"] <= 1
        grid_threshold = config["GRID_THRESHOLD"] * grid_height * grid_width
    else:
        grid_threshold = config["GRID_THRESHOLD"]
    mask_threshold = config["MASK_THRESHOLD"]
    assert mask_threshold >= 0 and mask_threshold <= 1

    mask = mask > mask_threshold
    depth /= 1000  # mm->m

    filtered = np.multiply(mask, depth)
    grids = (
        filtered.reshape(grid_num_h, grid_height, grid_num_w, grid_width)
        .transpose(0, 2, 1, 3)
        .reshape(grid_num_h * grid_num_w, grid_height * grid_width)
    )
    non_zero_num = np.where(grids > 0, np.ones_like(grids), np.zeros_like(grids)).sum(
        axis=1
    )
    non_zero_num[non_zero_num == 0] = -1  # In case of getting nan
    z = grids.sum(axis=1) / non_zero_num  # To get the mean value of the nonzeros

    label = np.where(
        non_zero_num > grid_threshold,
        np.ones_like(non_zero_num),
        np.zeros_like(non_zero_num),
    )  # 0:background 1:obstacle

    out = np.stack((label, z), axis=1)
    return out.flatten()
