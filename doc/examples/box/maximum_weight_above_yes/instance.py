import packingsolver.box as psb

instance_builder = psb.InstanceBuilder()
instance_builder.set_objective(psb.Objective.BinPacking)
instance_builder.add_bin_type(10, 20, 30, copies=10)
instance_builder.add_item_type(
        10,
        10,
        10,
        weight=100,
        maximum_weight_above=100,
        copies=6,
)
instance = instance_builder.build()
