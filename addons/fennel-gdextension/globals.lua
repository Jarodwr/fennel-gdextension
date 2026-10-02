gd.export_typed = function(type_or_class, default_val)
	if default_val ~= nil then
		return gd.export({type_or_class, default = default_val})
	else
		return gd.export(type_or_class)
	end
end

gd.rpc = function(...)
	local config = gd.Dictionary {
		rpc_mode = gd.MultiplayerAPI.RPC_MODE_AUTHORITY,
		transfer_mode = gd.MultiplayerPeer.TRANSFER_MODE_UNRELIABLE,
		call_local = false,
		channel = 0,
	}
	for i = 1, select("#", ...) do
		local value = select(i, ...)
		if value == "any_peer" then
			config.rpc_mode = gd.MultiplayerAPI.RPC_MODE_ANY_PEER
		elseif value == "authority" then
			config.rpc_mode = gd.MultiplayerAPI.RPC_MODE_AUTHORITY
		elseif value == "call_remote" then
			config.call_local = false
		elseif value == "call_local" then
			config.call_local = true
		elseif value == "unreliable" then
			config.transfer_mode = gd.MultiplayerPeer.TRANSFER_MODE_UNRELIABLE
		elseif value == "unreliable_ordered" then
			config.transfer_mode = gd.MultiplayerPeer.TRANSFER_MODE_UNRELIABLE_ORDERED
		elseif value == "reliable" then
			config.transfer_mode = gd.MultiplayerPeer.TRANSFER_MODE_RELIABLE
		elseif type(value) == "number" then
			config.channel = value
		end
	end
	return config
end
