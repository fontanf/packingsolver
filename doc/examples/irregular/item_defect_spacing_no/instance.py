import packingsolver.irregular as psi

instance_builder = psi.InstanceBuilder()
instance_builder.set_objective(psi.Objective.BinPackingWithLeftovers)
bin_type_id = instance_builder.add_bin_type(psi.build_rectangle(0, 160, 0, 120), copies=1)
defect_id = instance_builder.add_defect(bin_type_id, -1, psi.ShapeWithHoles(psi.build_shape([(40, 40), (60, 40), (40, 60)])))
instance_builder.set_item_defect_minimum_spacing(bin_type_id, defect_id, 0)
instance_builder.add_bin_type(psi.build_rectangle(0, 160, 0, 120), copies=2)
instance_builder.add_item_type(
        psi.build_shape([(0, 0), (40, 0), (0, 40)]),
        allowed_rotations=[(0, 0, False), (90, 90, False), (180, 180, False), (270, 270, False)],
        copies=23,
)
instance = instance_builder.build()
