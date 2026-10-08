import packingsolver.onedimensional as pso

instance_builder = pso.InstanceBuilder()
instance_builder.set_objective(pso.Objective.BinPacking)
instance_builder.add_bin_type(500, copies=10)
instance_builder.add_item_type(200, maximum_stackability=3, copies=3)
instance_builder.add_item_type(100, maximum_stackability=100, copies=4)
instance = instance_builder.build()
