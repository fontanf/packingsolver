import packingsolver.irregular as psi

instance_builder = psi.InstanceBuilder()
instance_builder.set_objective(psi.Objective.BinPacking)
instance_builder.add_bin_type(psi.build_rectangle(0, 80, 0, 60), copies=2)
instance_builder.add_item_type(
        psi.build_shape([(0, 0), (40, 0), (40, 20), (20, 20), (20, 60), (0, 60)]),
        allowed_rotations=[(0, 0, False), (0, 0, True)],
        copies=2,
)
instance_builder.add_item_type(
        psi.build_rectangle(0, 38, 0, 38),
        allowed_rotations=[(0, 0, False)],
        copies=1,
)
instance = instance_builder.build()
