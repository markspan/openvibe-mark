-- ug-trigger-stimulator.lua
--
-- Emits stimulation codes in the hardware-trigger range (values 1-255),
-- encoded as OVTK_StimulationId_Label(n) = 0x00008100 | n.
-- These are the only codes the UG acquisition server and Signal display (UG)
-- will accept and display.
--
-- Edit the two constants below, then point the Lua Stimulator box at this file.

-- ── USER CONFIGURATION ────────────────────────────────────────────────────────

TRIGGER_CODES    = { 1, 2, 3, 10, 20, 100, 255 }  -- codes to cycle through (1-255)
TRIGGER_INTERVAL = .2                              -- seconds between triggers

-- ── END OF CONFIGURATION ──────────────────────────────────────────────────────

function initialize(box)
	dofile(box:get_config("${Path_Data}") .. "/plugins/stimulation/lua-stimulator-stim-codes.lua")

	-- Validate codes
	trigger_codes = {}
	for _, v in ipairs(TRIGGER_CODES) do
		local n = math.floor(v)
		if n >= 1 and n <= 255 then
			table.insert(trigger_codes, n)
		else
			io.write(string.format("[ug-trigger] WARNING: code %d out of range 1-255, skipped\n", n))
		end
	end

	if #trigger_codes == 0 then
		io.write("[ug-trigger] ERROR: no valid codes — defaulting to {1}\n")
		trigger_codes = { 1 }
	end

	trigger_interval = (TRIGGER_INTERVAL > 0) and TRIGGER_INTERVAL or 2.0

	code_index     = 1
	next_send_time = -1   -- set on first process tick

	io.write(string.format("[ug-trigger] %d codes loaded, interval = %.3f s\n",
		#trigger_codes, trigger_interval))
end

function uninitialize(box)
	io.write("[ug-trigger] stopped\n")
end

function process(box)
	while box:keep_processing() do
		local t = box:get_current_time()

		if next_send_time < 0 then
			next_send_time = t + trigger_interval
		end

		if t >= next_send_time then
			local code    = trigger_codes[code_index]
			local stim_id = OVTK_StimulationId_LabelStart + code   -- 0x00008100 + code

			box:send_stimulation(1, stim_id, t, 0)
			io.write(string.format("[ug-trigger] t=%.3f  code=%d  id=0x%08x\n", t, code, stim_id))

			code_index     = (code_index % #trigger_codes) + 1
			next_send_time = next_send_time + trigger_interval
		end

		box:sleep()
	end
end
