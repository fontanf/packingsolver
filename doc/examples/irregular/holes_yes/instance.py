import packingsolver.irregular as psi

instance_builder = psi.InstanceBuilder()
instance_builder.set_objective(psi.Objective.BinPacking)
instance_builder.add_bin_type(psi.build_rectangle(0, 40, 0, 40), copies=2)
instance_builder.add_item_type(
        psi.ShapeWithHoles(psi.build_shape([(0, 0), (40, 0), (40, 30), (30, 40), (0, 40)]), [psi.build_shape([(10, 10), (30, 10), (20, 30)])]),
        copies=1,
)
instance_builder.add_item_type(
        psi.build_shape([(0, 0), (12, 0), (6, 12)]),
        copies=1,
)
instance = instance_builder.build()
