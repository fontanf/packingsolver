import packingsolver.onedimensional as pso

instance_builder = pso.InstanceBuilder()
instance_builder.set_objective(pso.Objective.BinPacking)
instance_builder.add_bin_type(800, copies=10)
instance_builder.add_item_type(200, weight=100, copies=4)
instance = instance_builder.build()
