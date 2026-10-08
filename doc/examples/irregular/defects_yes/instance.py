import packingsolver.irregular as psi

instance_builder = psi.InstanceBuilder()
instance_builder.set_objective(psi.Objective.BinPacking)
bin_type_id = instance_builder.add_bin_type(psi.build_rectangle(0, 60, 0, 60), copies=1)
instance_builder.add_defect(bin_type_id, -1, psi.ShapeWithHoles(psi.build_rectangle(5, 15, 5, 15)))
instance_builder.add_bin_type(psi.build_rectangle(0, 60, 0, 60), copies=1)
instance_builder.add_item_type(
        psi.build_shape([(0, 0), (40, 0), (40, 20), (20, 20), (20, 60), (0, 60)]),
        allowed_rotations=[(0, 0, False), (90, 90, False), (180, 180, False), (270, 270, False)],
        copies=2,
)
instance = instance_builder.build()
