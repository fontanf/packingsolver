import packingsolver.irregular as psi

instance_builder = psi.InstanceBuilder()
instance_builder.set_objective(psi.Objective.BinPacking)
instance_builder.add_bin_type(psi.build_rectangle(0, 180, 0, 160), copies=3)
instance_builder.add_item_type(
        psi.build_shape([(0, 0), (80, 0), (80, 20), (0, 20)]),
        allowed_rotations=[(0, 0, False), (90, 90, False), (180, 180, False), (270, 270, False)],
        copies=2,
)
instance_builder.add_item_type(
        psi.build_shape([(0, 0), (40, 0), (40, 40), (0, 40)]),
        allowed_rotations=[(0, 0, False)],
        copies=2,
)
instance_builder.add_item_type(
        psi.build_shape([(0, 0), (60, 0), (60, 20), (40, 20), (40, 40), (20, 40), (20, 20), (0, 20)]),
        allowed_rotations=[(0, 0, False), (90, 90, False), (180, 180, False), (270, 270, False)],
        copies=2,
)
instance_builder.add_item_type(
        psi.build_shape([(20, 0), (60, 0), (60, 20), (40, 20), (40, 40), (0, 40), (0, 20), (20, 20)]),
        allowed_rotations=[(0, 0, False), (90, 90, False), (180, 180, False), (270, 270, False)],
        copies=2,
)
instance_builder.add_item_type(
        psi.build_shape([(0, 0), (40, 0), (40, 20), (60, 20), (60, 40), (20, 40), (20, 20), (0, 20)]),
        allowed_rotations=[(0, 0, False), (90, 90, False), (180, 180, False), (270, 270, False)],
        copies=2,
)
instance_builder.add_item_type(
        psi.build_shape([(0, 0), (40, 0), (40, 20), (20, 20), (20, 60), (0, 60)]),
        allowed_rotations=[(0, 0, False), (90, 90, False), (180, 180, False), (270, 270, False)],
        copies=2,
)
instance_builder.add_item_type(
        psi.build_shape([(0, 0), (40, 0), (40, 60), (20, 60), (20, 20), (0, 20)]),
        allowed_rotations=[(0, 0, False), (90, 90, False), (180, 180, False), (270, 270, False)],
        copies=2,
)
instance_builder.add_item_type(
        psi.build_shape([(20, 0), (40, 0), (40, 20), (60, 20), (60, 40), (40, 40), (40, 60), (20, 60), (20, 40), (0, 40), (0, 20), (20, 20)]),
        allowed_rotations=[(0, 0, False)],
        copies=3,
)
instance = instance_builder.build()
