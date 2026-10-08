import packingsolver.irregular as psi

instance_builder = psi.InstanceBuilder()
instance_builder.set_objective(psi.Objective.OpenDimensionXY)
instance_builder.set_open_dimension_xy_aspect_ratio(1)
instance_builder.add_bin_type(psi.build_rectangle(0, 300, 0, 300))
instance_builder.add_item_type(
        psi.build_shape([(-12.5, 50), (-6.25, 50), (-20, 25), (-10, 25), (-35, 0), (-7.5, 0), (-7.5, -20), (7.5, -20), (7.5, 0), (35, 0), (10, 25), (20, 25), (6.25, 50), (12.5, 50), (0, 80)]),
        allowed_rotations=[(0, 360, False)],
        copies=7,
)
instance = instance_builder.build()
