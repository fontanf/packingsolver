import packingsolver.rectangleguillotine as psg

instance_builder = psg.InstanceBuilder()
instance_builder.set_objective(psg.Objective.BinPacking)
instance_builder.set_number_of_stages(2)
bin_type_id = instance_builder.add_bin_type(10, 10)
instance_builder.add_defect(bin_type_id, 4, 7, 2, 2)
instance_builder.add_bin_type(10, 10, copies=2)
instance_builder.add_item_type(5, 5, oriented=True)
instance_builder.add_item_type(5, 6, oriented=True)
instance = instance_builder.build()
