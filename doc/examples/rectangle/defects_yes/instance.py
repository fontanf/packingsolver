import packingsolver.rectangle as psr

instance_builder = psr.InstanceBuilder()
instance_builder.set_objective(psr.Objective.BinPackingWithLeftovers)
bin_type_id = instance_builder.add_bin_type(800, 500)
instance_builder.add_defect(bin_type_id, 450, 150, 50, 200)
instance_builder.add_bin_type(800, 500)
instance_builder.add_item_type(400, 200, oriented=True, copies=3)
instance = instance_builder.build()
