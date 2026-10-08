import packingsolver.box as psb

instance_builder = psb.InstanceBuilder()
instance_builder.set_objective(psb.Objective.BinPacking)
instance_builder.add_bin_type(10, 10, 10, copies=2)
instance_builder.add_item_type(10, 10, 6, rotations=[psb.Rotation.XYZ])
instance_builder.add_item_type(
        10,
        4,
        6,
        rotations=[psb.Rotation.XYZ, psb.Rotation.XZY],
)
instance = instance_builder.build()
