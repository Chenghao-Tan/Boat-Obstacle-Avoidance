import numpy as np


def z2xy_coefficient(
    grid_height,
    grid_width,
    grid_num_h,
    grid_num_w,
    intrinsic_matrix=np.array(
        [
            [492.23822021, 0.0, 320.57855225],
            [0.0, 492.23822021, 181.67709351],
            [0.0, 0.0, 1.0],
        ]
    ),
):
    # Center of each grid
    grid_cx = np.linspace(
        (grid_width - 1) / 2,
        grid_width * grid_num_w - 1 - (grid_width - 1) / 2,
        grid_num_w,
    )
    grid_cy = np.linspace(
        (grid_height - 1) / 2,
        grid_height * grid_num_h - 1 - (grid_height - 1) / 2,
        grid_num_h,
    )
    grid_cx, grid_cy = np.meshgrid(grid_cx, grid_cy)  # To 2D mesh

    # Focal length
    fx = intrinsic_matrix[0, 0]
    fy = intrinsic_matrix[1, 1]
    # Center of the image
    cx = intrinsic_matrix[0, 2]
    cy = intrinsic_matrix[1, 2]

    # Projection coefficient
    x_coefficient = ((grid_cx - cx) / fx).flatten()
    y_coefficient = ((cy - grid_cy) / fy).flatten()

    return x_coefficient, y_coefficient


def z2xy_coefficient_fov(grid_height, grid_width, grid_num_h, grid_num_w, hfov=69.0):
    # WARNING: Use HFOV only (assuming fx==fy)

    # Center of each grid
    grid_cx = np.linspace(
        (grid_width - 1) / 2,
        grid_width * grid_num_w - 1 - (grid_width - 1) / 2,
        grid_num_w,
    )
    grid_cy = np.linspace(
        (grid_height - 1) / 2,
        grid_height * grid_num_h - 1 - (grid_height - 1) / 2,
        grid_num_h,
    )
    grid_cx, grid_cy = np.meshgrid(grid_cx, grid_cy)  # To 2D mesh

    # H&W of the image
    height = grid_height * grid_num_h
    width = grid_width * grid_num_w
    # Assuming fx==fy
    fx_r = np.tan(hfov / 180 * np.pi / 2) / (width / 2)  # Reciprocal of fx
    fy_r = fx_r  # Reciprocal of fy
    # Center of the image
    cx = width / 2
    cy = height / 2

    # Projection coefficient
    x_coefficient = ((grid_cx - cx) * fx_r).flatten()
    y_coefficient = ((cy - grid_cy) * fy_r).flatten()

    return x_coefficient, y_coefficient
