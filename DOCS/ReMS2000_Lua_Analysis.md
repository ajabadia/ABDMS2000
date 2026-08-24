# ReMS2000 — Análisis del Código Lua y Algoritmos MIDI/SysEx

*Este documento contiene el código Lua extraído del panel ReMS2000 (desarrollado por inteyes en Ctrlr), que implementa el protocolo completo de comunicación, desempaquetado de datos y gestión de parches del Korg MS2000.*

---

## `table_dump`

```lua
-- @1.1
--
-- Print table contents
--
function table_dump(table)
	for key,value in ipairs(table) do
		_DBG ("KEY= ["..key.."]")

		if (type(value) == "table") then
			table_dump(value)
		elseif (type(value) == "nil") then
			_DBG (" = NIL")
		else
			what (value)
		end
	end
end
```

---

## `setGlobalVars`

```lua
function setGlobalVars()

	sharedValues = {}

	sharedValues.selectedPreset			= 1				-- Selected program on the panel
	sharedValues.selectedBank 			= 1 			-- Selected bank on the panel
	sharedValues.synthPreset			= 0 			-- Selected program on the MS2000
	sharedValues.synthBank 				= 0 			-- Selected bank on the MS2000
	sharedValues.synthProgram			= 0 			-- Calculated synth program number (bank + preset) for writing
	sharedValues.selectedTimbre 		= 0
	sharedValues.selectedSequence		= 0
	sharedValues.midiActivity			= 0 			-- Midi activity flag for blinking the indicator
	sharedValues.reachStatus			= 0			
	sharedValues.timbreMode 			= tmSynth		-- Current global mode - synthesizer / vocoder
	sharedValues.voiceMode				= vmUndefined	-- More specific mode description
	sharedValues.deviceStatus 			= dsOffline		-- Device status according to the panel
	sharedValues.operationMode			= omDefault		-- Synthesizer operation mode (Program, Edit, Global)
	sharedValues.hintMessage			= ""
	sharedValues.saveToRamEnabled		= 0				-- Flag to show if preset belong or not to specific program of the bank
	sharedValues.allowChangeSeq			= false			-- Flag to prevent auto sequence changing			
	sharedValues.isSequencerDragging	= false
	sharedValues.sequencerStartY		= 0
	sharedValues.sequencerKnob			= 0				-- Knob to be affected by sequencer dragging
	sharedValues.sequencerKnobValue		= 0				-- Value at the moment of drag begins
	sharedValues.applySettingsOnCatch	= false			-- Required for settings merging routine
	sharedValues.ignoreSettingsButton	= true			-- Workaround for strange behaviour when settings page was not closed before saving state
	sharedValues.customBGColor			= SEQ_BACKGROUND
	sharedValues.playMessageTuple		= {}			-- Program Play mode can send 3 messages in a row, but Ctrlr do not recognize them as multimessage
	sharedValues.resetDWGS				= true			-- Flag to process OSC1 Control2 bounds and formula
	sharedValues.restartRequired		= false			-- Flag to indicate if panel restart is required

	-- SysEx formula templates for non-visual components, will be overriden in certain methods
	sharedValues.osc1SEValues 	= {0xF0, 0x42, 0x00, 0x58, 0x41, 0x49, 0x00, 0x00, 0x00, 0xF7}
	sharedValues.osc2SEValues 	= {0xF0, 0x42, 0x00, 0x58, 0x41, 0x4D, 0x00, 0x00, 0x00, 0xF7}
	sharedValues.oscModSEValues = {0xF0, 0x42, 0x00, 0x58, 0x41, 0x4E, 0x00, 0x00, 0x00, 0xF7}
	sharedValues.filterSEValues = {0xF0, 0x42, 0x00, 0x58, 0x41, 0x54, 0x00, 0x00, 0x00, 0xF7}
	sharedValues.LFO1SEValues 	= {0xF0, 0x42, 0x00, 0x58, 0x41, 0x68, 0x00, 0x00, 0x00, 0xF7}
	sharedValues.LFO2SEValues 	= {0xF0, 0x42, 0x00, 0x58, 0x41, 0x6D, 0x00, 0x00, 0x00, 0xF7}

	panelSettings = {}

	panelSettings.sendProgOnStartup	= 0
	panelSettings.sendOnProgChange	= 0
	panelSettings.reqProgOnChange	= 0	-- Request program on change synthesizer's program number
	panelSettings.autocheckLCDMode	= 0	-- Force LCD mode on every poll cycle
	panelSettings.disableWarnings	= 0	-- Disable all warning dialogs. Might be dangerous
	panelSettings.clockSource		= 2
	panelSettings.localMode			= 1
	panelSettings.continuousPolling	= 1
	panelSettings.selectorsSource	= pbsPanel	-- Flag to recognize where to cycle program - on the panel, or on the synthesizer side
	panelSettings.selectedSkin		= csDefault

	-- Skin colors
	skinColors = {}

	-- Init default colors
	defaultScheme()

	-- Timer flags
	timerFlags = {}

	-- Flags to indicate if some kind of data is expected or not
	-- Data that not expected will be ignored on input
	timerFlags.waitForSingleProgram	= false
	timerFlags.waitForBulkDump		= false
	timerFlags.waitForSettings		= false
	timerFlags.waitForWriteReply	= false

	-- Patch bank
	presetBank = initPresetBank()

	-- Current program data buffer. It's INIT program by default
	dataBuffer = copyTable(presetBank[1][1])

	-- Vocoder buffer
	vocoderBuffer = initVocoderBuffer()

	-- Timbre clipboard
	timbreClipboard = {}

	-- Sequence clipboard
	seqClipboard = {}

end
```

---

## `setGlobalConstants`

```lua
function setGlobalConstants()

	panelVersion = "1.3.4"

	-- Color definitions
	COMP_DISABLED_ALPHA	= 0.5
	COMP_ENABLED_ALPHA	= 1

	-- LCD
	LCD_BASE		= Colour(0xFF333333)
	LCD_BACKLIGHT  	= Colour(0xFFc4e283)
	LCD_DIGITS 	   	= Colour(0xFFb5ca87)
	LCD_TEXT		= Colour(0xFF202020)
	LCD_GLOW_START 	= Colour(0x25FFFFFF)
	LCD_GLOW_END   	= Colour(0x00FFFFFF)

	-- Icons
	ICON_ORANGE	= Colour(0xFFC6A34E)
	ICON_GREEN	= Colour(0xFF66BB66)
	ICON_RED	= Colour(0xFFDD6666)

	-- Drawing colors
	COLOR_SURFACE_LINE		= Colour(0xFFBABABA)
	COLOR_SURFACE_LINE_DARK	= Colour(0xFF9A9A9A) 
	COLOR_GREY_TEXT			= Colour(0xFF9A9A9A)
	COLOR_TRANSPARENT		= Colour(0x00000000)
	COLOR_PANEL_BG			= Colour(0xFF334657)
	COLOR_SETUP_BG			= Colour(0xFA334657)
	COLOR_PANEL_BG_BLK		= Colour(0xFF353535)
	COLOR_SETUP_BG_BLK		= Colour(0xFA353535)
	COLOR_VOCODER_LABEL		= Colour(0xFFAAAAAA)
	COLOR_COMBO_TEXT		= Colour(0xFFA6914F)
	COLOR_BUFFER_COPY		= Colour(0xFFA6524A)
	COLOR_BUFFER_COPY_V		= Colour(0xFF4A85A6)
	COLOR_BUFFER_TEXT		= Colour(0xFFFAFAFA)
	COLOR_BUFFER_TEXT_EMPTY	= Colour(0xFFBABABA)
	COLOR_EMERGENCY			= Colour(0xFF993333)
	COLOR_SEQ_BG_BLACK		= Colour(0xFF1A2024)
	COLOR_GROUPBOX_OUTLINE	= Colour(0xFFA3A3A3)
	COLOR_GROUPBOX_LABEL	= Colour(0xFFBABABA)

	-- Sequencer
	SEQ_BACKGROUND		= Colour(0xFF2A3034)
	SEQ_BG_ALTER		= Colour(0xFF252A2E)
	SEQ_BG_ALTER_BLACK	= Colour(0xFF202529)
	SEQ_NUMBER_ONE   	= Colour(0xFF00FF00)
	SEQ_NUMBER_TWO   	= Colour(0xFF00CCFF)
	SEQ_NUMBER_THREE 	= Colour(0xFFFFCC00)
	SEQ_GRAYED_OUT		= Colour(0xFFBABABA)

	-- Operation modes
	omDefault = 0
	omLCD 	  = 1
	omGlobal  = 2

	-- Device status
	dsOnline	= 0
	dsBusy		= 1
	dsOffline	= 2
	dsError		= 3

	-- Error codes
	errMaxValExceeded = 0

	-- Timbre modes
	tmSynth		= 0
	tmVocoder	= 1

	-- Voice modes
	vmUndefined	= -1
	vmSingle	= 0
	vmSplit		= 1
	vmDual		= 2
	vmVocoder	= 3

	-- Destination program buffer
	dbSynth		= 0
	dbVocoder	= 1

	-- Color scheme
	csDefault	= 0
	csBlack		= 1
	csNordLead	= 2
	csJP8080	= 3

	-- Supported file extensions
	SUPPORTED_EXT_MASK		= "*.syx;*.mid"
	SUPPORTED_EXT_MASK_ALT	= "*.syx;*.mid;*.prg"

	-- Bank \ program selection source
	pbsPanel	= 0
	pbsSynth	= 1

	-- Program data values
	DATA_PREAMBLE_BYTES		= 5		-- SysEx header
	COMMON_DATA_SIZE		= 38	-- Shared values for both timbres

	-- Dump size values
	-- Raw MIDI data
	SINGLE_PROGRAM_SIZE		= 297	-- (291 + 5 bytes SysEx header + F7)
	GLOBAL_DATA_SIZE		= 235	-- (229 + SysEx data)
	PROGRAM_BANK_DUMP_SIZE	= 37163	-- (37157 + SysEx data)
	ALL_DATA_DUMP_SIZE		= 37392 -- (37386 + SysEx data)
	MKSINGLE_PROGRAM_SIZE	= SINGLE_PROGRAM_SIZE + 2 --(MicroKorg program size)

	-- Special cases
	HANDSON_DUMP_SIZE		= 37395

	-- MIDI-to-Program converted data size
	SINGLE_PROGRAM_INT_SIZE = 254	

	TIMBRE_DATA_SIZE		= 108
	TIMBRE_ONE_STARTBYTE	= DATA_PREAMBLE_BYTES + COMMON_DATA_SIZE + 1
	TIMBRE_TWO_STARTBYTE	= TIMBRE_ONE_STARTBYTE + TIMBRE_DATA_SIZE

	-- Buffer copy values
	SEQUENCE_STARTBYTE_DISP		= 53
	SEQUENCE_DATA_SIZE			= 55
	VOCODER_SEQDATA_STARTBYTE	= 47
	VOCODER_SEQDATA_SIZE		= 32

	-- Timbre values
	OSC1_WAVEFORM_DISP	= 7
	OSC2_WAVEFORM_DISP	= 12 -- WARNING, packed byte, bits 0~1
	OSCMODULATION_DISP	= 12 -- WARNING, packed byte, bits 4~5
	FILTER_TYPE_DISP	= 19
	LFO1_TYPE_DISP		= 38 -- WARNING, packed byte, bits 0~1
	LFO2_TYPE_DISP		= 41 -- WARNING, packed byte, bits 0~1

	-- Vocoder values
	OSC1_WAVEFORM_VCD_DISP	= 8
	FILTER_TYPE_VCD_DISP	= 22
	LFO1_TYPE_VCD_DISP		= 41
	LFO2_TYPE_VCD_DISP		= 44

	SYSEX_VAL_DIFF		= 272	-- Difference between modulator numbers on different layers
	SYSEX_VAL_DIFF_ALT	= 400

	-- Dump type
	dtInvalidSz	= -1
	dtProgBank	= 0
	dtAllData	= 1
	dtHandson	= 2

	-- PopUp result values
	prOpenProgram		= 1
	prOpenDump			= 2
	prSaveProgram		= 10
	prSaveDump			= 11
	prSaveToRAM			= 12
	prRenameProgram		= 15
	prInitProgram		= 20
	prInitBank			= 21
	prRequestProgram	= 30
	prWriteProgram		= 31
	prRequestSysexDump	= 40
	prWriteSysexDump	= 41

	-- Timer values
	POLL_TIMER			= 10000	-- Constant synth availability polling
	SHOWHINT_TIMER		= 15000	-- How long hint will be shown
	STARTUP_TIMER		= 250	-- Delay before applying all startup data
	BLINKMIDI_TIMER		= 75	-- How long midi indicator shown
	WAIT_PROGRAM_TIMER	= 3000	-- Wait for program to be received
	WAIT_BANK_TIMER		= 22000 -- Wait for program bank to be received
	POLLSTATE_TIMER		= 3000	-- Timeout for synth mode request
	WAITFORSET_TIMER	= 3000	-- Timeout for synth settings request
	DELAY_PROG_REQUEST	= 300	-- Delay before run request

	STARTUP_TIMER_ID		= 1
	HINT_TIMER_ID			= 10
	POLL_TIMER_ID			= 20
	BLINKMIDI_TIMER_ID		= 30
	WAIT_PROGRAM_TIMER_ID	= 40
	WAIT_BANK_TIMER_ID		= 41
	POLLSTATE_TIMER_ID		= 50
	WAITFORSET_TIMER_ID		= 80
	DELAY_PROG_REQUEST_ID	= 90
	WAITFORWRITE_REPLY_ID	= 100 -- Will indicate if write ok reply was or was not received

	DEFINE_DEBUG = false

end
```

---

## `startupSequence`

```lua
function startupSequence()

	-- Starting up the panel here
	panelReady = false

	-- Prevent panel from sending MIDI-messages while initialization goes
	mutePanelOut(true)

	setGlobalConstants()
	setGlobalVars()
	restoreGlobalSettings()
end
```

---

## `calculateLSBMSB`

```lua
function calculateLSBMSB(calcVal, fullByte, nibbleLSB)
	
	local fB = 0x07
	local lsM = 0x7F

	if fullByte ~= nil then
		if fullByte then
			fB = 0x08

			if nibbleLSB ~= nil then
				if not nibbleLSB then
					lsM = 0xFF
				end
			end
		end
	end

    local lsb = bit.band(calcVal, lsM)
    local msb = bit.rshift(calcVal, fB)

	return {lsb, msb}
end

function restoreValueFromLSMS(msb, lsb)

	return msb * 0x80 + lsb
end
```

---

## `midiToProgramData`

```lua
function midiToProgramData(rawPatchData, seekBytes)
	
	local dataSize = #rawPatchData - seekBytes - 1
	local blockSize = 8
	local dataBlockCount = math.ceil(dataSize / blockSize)
	local bytesRemain = blockSize
	local i, j

	local msbBytes = {}
	local dataChunk = {}
	local processedData = {}
	
	-- Insert header to the final table as is
	for i = 1, seekBytes do
		table.insert(processedData, rawPatchData[i])
	end

	-- Process data, according to the KORG's midi implementation doc
	-- It uses 1 + 7 "7BIT byte" chunks to store 1MSB + 7LSB bytes (in my understanding, at least)
	-- Zeroes and Ones from 1st byte used to indicate MSB value of each byte in a chunk
	for j = 0, dataBlockCount - 1 do

		if j == dataBlockCount - 1 then
			bytesRemain = dataSize - ((dataBlockCount - 1) * blockSize)
		end

		-- Getting 8 bytes of data to make calculations
		for i = 0, bytesRemain - 1 do
			table.insert(dataChunk, rawPatchData[seekBytes + 1 + (j * blockSize) + i])
		end

		-- Convert 1st byte to "1"s and "0"s
		msbBytes = Dec2Bin(dataChunk[1])

		-- Apply MSB bits for mapped bytes
		for i = 2, bytesRemain do

			dataChunk[i] = dataChunk[i] + (0x80 * msbBytes[i - 1])
			table.insert(processedData, dataChunk[i])
		end

		dataChunk = {}
 	end

	-- End of SysEx
	table.insert(processedData, 0xF7)

	return processedData
end
```

---

## `programToMIDIData`

```lua
function programToMIDIData(bufferData, seekBytes)
	
	local programDataSize = #bufferData - seekBytes - 1
	local blockSize = 7
	local dataBlockCount = math.ceil(programDataSize / 7)

	local i, j
	local lsms = {}
	local msbBytes = {}
	local dataChunk = {}
	local bytesRemain = 7
	local processedData = {}

	-- Insert header to the final table as is
	for i = 1, seekBytes do
		table.insert(processedData, bufferData[i])
	end

	-- Constructing MS2000-compatible MIDI data format to store or send
	-- All values must be pretended as LSB and MSB, MSB data moved to a single byte
	-- and placed before the data chunk (which is 7 bytes long)

	for i = 0, dataBlockCount - 1 do

		if i == dataBlockCount - 1 then
			bytesRemain = programDataSize - ((dataBlockCount - 1) * blockSize)
		end

		-- Construct MSB-bits array
		for j = 0, bytesRemain - 1 do

			lsms = calculateLSBMSB(bufferData[seekBytes + 1 + (i * blockSize) + j])

			-- Collect MSB bit
			table.insert(msbBytes, lsms[2])

			-- Insert LSB Value into result table
			table.insert(dataChunk, lsms[1])
		end

		-- Insert "shared" MSB value as 1st byte of this chunk
		table.insert(processedData, Bin2Dec(msbBytes))

		for j = 1, #dataChunk do
			table.insert(processedData, dataChunk[j])
		end

		dataChunk = {}
		msbBytes = {}
	end

	-- End of SysEx
	table.insert(processedData, 0xF7)

	return processedData
end
```

---

## `binConversion`

```lua
function Bin2Dec(bitArr)

	local result = 0

	-- Since max value is 127, it cannot be more than 7 bits long
	if #bitArr > 7 then
		return result
	end

	local invBytes = {}
	local i

	for i = #bitArr, 1, -1 do
		table.insert(invBytes, bitArr[i])
	end

	for i = 1, #invBytes do
		result = result + (invBytes[i] * 2 &#94; (#invBytes - i))
	end

	return result
end

function Dec2Bin(num)
	
	local result = {0, 0, 0, 0, 0, 0, 0}
	local bufTab = {}

	if (num > 0x7F) or (num == 0) then
		-- Wrong values passed, or it's zero
		return result
	end

	local invertBytes = {}
	local divided, divider = num, num
	local i

	while divided > 1 do

		divider = math.floor(divided / 2)

		if divided - (divider * 2) > 0 then
			table.insert(bufTab, 1)
		else
			table.insert(bufTab, 0)
		end

		divided = divider
 	end

	table.insert(bufTab, divider)

	for i = 1, #bufTab do
		result[i] = bufTab[i]
	end

	return result
end
```

---

## `memBlockToTable`

```lua
function memBlockToTable(mbBytes)
	
	local i

	local mbData = {}

	for i = 0, mbBytes:getSize() - 1 do
		table.insert(mbData, mbBytes:getByte(i))
	end

	return mbData
end
```

---

## `bankIDToName`

```lua
function bankIDToName(bankID)
	
	return string.char(64 + bankID)
end
```

---

## `packedByteUtils`

```lua
function extractPackByte(byteValue, firstBit, lastBit)

	local lBit

	if lastBit == nil then
		lBit = firstBit
	else
		lBit = lastBit
	end

	local valSize = lBit - firstBit + 1

	-- The simplest way to get bit-based value to me
	return bit.band(bit.rshift(byteValue, firstBit), (2 &#94; valSize) - 1)
end

function packBitsToByte(srcValue, packValue, firstBit, lastBit)

	local lBit

	if lastBit == nil then
		lBit = firstBit
	else
		lBit = lastBit
	end

	-- What I do is subtract shifted value, which is stored in
	-- certain bits to make them filled with zeroes
	-- Then write new bits with the "OR" operation

	local existingValue = extractPackByte(srcValue, firstBit, lBit)

	local prepValue = srcValue - bit.lshift(existingValue, firstBit)

	local bitsToPack = bit.lshift(packValue, firstBit)

	--console(string.format("valueEx=%d, shiftedV=%d, srcV=%d, src-shift=%d, packVal=%d, shift=%d, shiftedPV=%d, fb=%d, lb=%d", existingValue, 
	--	bit.lshift(existingValue, firstBit), srcValue, prepValue, packValue, 8 - firstBit - valSize, bitsToPack, firstBit, lastBit))

	-- ([..0.. OR value] bits)
	return bit.bor(prepValue, bitsToPack)
end
```

---

## `normalizeSysExDumpData`

```lua
function normalizeSysExDumpData(dataTable)
	
	-- Throwing away data which is not belong to "midi dump data"
	-- Filter bytes - block must start with F0 and end with F7

	-- Other data will be cutted out

	local processedData = {}

	local i
	local readState = false
	local dataSize = #dataTable

	for i = 1, dataSize do

		if (not readState) then

			if dataTable[i] == 0xF0 then

				readState = true
				table.insert(processedData, dataTable[i])
			end
		else

			if dataTable[i] == 0xF7 then

				readState = false
			end

			table.insert(processedData, dataTable[i])
		end
	end

	return processedData
end
```

---

## `getMidiInOut`

```lua
function getMidiInOut(getInput)

	local result = ""
	local midiCh = ""

	if getInput then
		result = panel:getProperty("panelMidiInputDevice")
		midiCh = panel:getProperty("panelMidiInputChannelDevice")

		if midiCh == "0" then
			midiCh = "All"
		end
	else
		result = panel:getProperty("panelMidiOutputDevice")
		midiCh = panel:getProperty("panelMidiOutputChannelDevice")
	end

	if result ~= "-- None" then

		result = result .. " : CH " .. midiCh
	end

	return result
end
```

---

## `bankListPopup`

```lua
function bankListPopup(load)
	
	local popupWin = PopupMenu()

	popupWin:addSectionHeader("Select bank:")
	popupWin:addSeparator()

	popupWin:addSubMenu("Bank A", constructPresetBankMenu(1, "A", load), true, Image(), false, 0)
	popupWin:addSubMenu("Bank B", constructPresetBankMenu(2, "B", load), true, Image(), false, 0)
	popupWin:addSubMenu("Bank C", constructPresetBankMenu(3, "C", load), true, Image(), false, 0)
	popupWin:addSubMenu("Bank D", constructPresetBankMenu(4, "D", load), true, Image(), false, 0)
	popupWin:addSubMenu("Bank E", constructPresetBankMenu(5, "E", load), true, Image(), false, 0)
	popupWin:addSubMenu("Bank F", constructPresetBankMenu(6, "F", load), true, Image(), false, 0)
	popupWin:addSubMenu("Bank G", constructPresetBankMenu(7, "G", load), true, Image(), false, 0)
	popupWin:addSubMenu("Bank H", constructPresetBankMenu(8, "H", load), true, Image(), false, 0)

	return popupWin
end
```

---

## `constructPresetBankMenu`

```lua
function constructPresetBankMenu(bankNumber, bankName, load)
	
	local popupWin = PopupMenu()
	local bankID = 100 * bankNumber

	if not load then
		bankID = bankID + 1000
	end

	popupWin:addSectionHeader(string.format("Preset bank %s:", bankName))
	popupWin:addSeparator()

	for i = 1, 16 do
		popupWin:addItem(bankID + i, string.format("%d. %s", i, getPresetNameByID(bankNumber, i)), true, false)
	end

	return popupWin
end
```

---

## `drawOsc1Waveforms`

```lua
function drawOsc1Waveforms(comp, g)
	
	drawSaw(comp, g, 22, 5, 10, 2)
	drawPulse(comp, g, 31, 5, 10, 2)
	drawTriangle(comp, g, 60, 5, 10, 2)
	drawSine(comp, g, 76, 5, 2)	
end
```

---

## `setOsc1Waveform`

```lua
function setOsc1Waveform(comp)

	local selectedWave = getModPropN(comp:getOwner(), "modulatorCustomIndex")

	setOsc1WaveformByValue(selectedWave)
end
```

---

## `calculateOscMod`

```lua
function calculateOscMod(ring, sync)
	
	if (ring or sync) == false then
		return 0
	elseif (ring == true) and (sync == false) then
		return 1
	elseif (ring == false) and (sync == true) then
		return 2
	else
		return 3
	end
end
```

---

## `processOscModData`

```lua
function processOscModData(modValue, muteOutput)
	
	local ring
	local sync
	local OscModAddress = TIMBRE_ONE_STARTBYTE + (sharedValues.selectedTimbre * TIMBRE_DATA_SIZE) + OSCMODULATION_DISP

	if modValue == 0 then
		ring, sync = false, false
	elseif modValue == 1 then
		ring, sync = true, false
	elseif modValue == 2 then
		ring, sync = false, true
	else
		ring, sync = true, true
	end

	setLightState("imgOsc2ModLamp0", ring)
	setLightState("imgOsc2ModLamp1", sync)

	dataBuffer[OscModAddress] = packBitsToByte(dataBuffer[OscModAddress], calculateOscMod(ring, sync), 4, 5)
end
```

---

## `paintSequencer`

```lua
function paintSequencer(comp, g)

	local i
	local roundRect = 3
	local canvasW = comp:getWidth()
	local canvasH = comp:getHeight()
	local canvasMid = canvasH / 2
	local figStartY = canvasMid
	local dashLen = 9
	local dashSpace = 1
	local dashCount = math.floor(canvasW / dashLen)
	local graphW = math.ceil(canvasW / 16)
	local maxGraphHeight = canvasMid * 0.75
	local drawProportion
	local lastStep = modByName("knobSeqLastStep"):getValue()

	g:setColour(skinColors.customBG)
	g:fillRoundedRectangle(0, 0, canvasW, canvasH, roundRect)

	-- Paint background, zebra-style
	g:setColour(skinColors.customBGAlter)

	for i = 1, 16 do
		if i % 2 > 0 then
			if i < 15 then
				g:fillRect(i * graphW, 0, graphW, canvasH)
			else
				g:fillRect(i * graphW, 0, graphW - roundRect, canvasH)
				g:fillRoundedRectangle(i * graphW, 0, graphW, canvasH, roundRect)
			end
		end
	end

	if sharedValues.timbreMode == tmSynth then

		drawProportion = maxGraphHeight / getSeqStepMaxVal(modByName(string.format("cbSeqKnob%d", 
													   	sharedValues.selectedSequence + 1)):getValue())[2]
	else
		if sharedValues.selectedSequence == 0 then
			maxGraphHeight = canvasH * 0.875

			-- Start bar from the bottom
			figStartY = canvasH
		end

		drawProportion = maxGraphHeight / getSeqStepMaxVal(sharedValues.selectedSequence)[2]
	end

	-- Dashed line only for +/- ranges
	g:setColour(skinColors.lineColor)

	-- Paint dashed line
	for i = 0, dashCount do

		if maxGraphHeight < canvasMid then
			g:fillRect(i * dashLen, canvasMid, dashLen - dashSpace, 1)
		else
			g:fillRect(i * dashLen, canvasMid * 1.125, dashLen - dashSpace, 1)
		end
	end

	local knobVal
	local graphEnd
	
	if sharedValues.selectedSequence == 0 then
		g:setColour(skinColors.seqOneBars)
	elseif sharedValues.selectedSequence == 1 then
		g:setColour(skinColors.seqTwoBars)
	else
		g:setColour(skinColors.seqThreeBars)
	end

	g:setFont(Font(10.0, 0))

	for i = 1, 16 do
		if i == lastStep + 1 then
			g:setColour(skinColors.seqGrayedOut)
		end

		knobVal = modByName(string.format("knobSeq%dStep%d", 
										  sharedValues.selectedSequence + 1, i)):getValue()

		if knobVal >= 0 then
			g:setOpacity(0.42 - skinColors.seqOpacityMinus)

			graphEnd = knobVal * drawProportion
			g:fillRect((i - 1) * graphW, figStartY - graphEnd, graphW - 1, graphEnd + 1)
			g:setOpacity(0.9)

			-- Paint these tiny value labels
			g:drawText(tostring(knobVal), (i - 1) * graphW, 
			           figStartY - graphEnd - 9, 20, 8, Justification(Justification.left), false)  
		else
			-- Negative values are more transparent
			g:setOpacity(0.30 - skinColors.seqOpacityMinus)

			graphEnd = math.abs(knobVal * drawProportion)
			g:fillRect((i - 1) * graphW, canvasMid, graphW - 1, graphEnd)
			g:setOpacity(0.9)

			g:drawText(tostring(knobVal), (i - 1) * graphW, 
			           canvasMid + graphEnd + 2, 20, 8, Justification(Justification.left), false)
		end
	end	

	g:setOpacity(1)
end
                       
```

---

## `paintSeq2BG`

```lua
function paintSeq2BG(mod, g)

	g:setColour(skinColors.customBG)
	g:fillRoundedRectangle(0, 0, mod:getWidth(), mod:getHeight(), 2)

	local i
	local spacer = 4
	local textH = 9

	local mainSeqRect = getCompProp("uiSequencerScreen", "componentRectangle")
	local mainSeqWidth = getComp("uiSequencerScreen"):getWidth()
	local mainSeqWLeft = tonumber(string.sub(mainSeqRect, 1, string.find(mainSeqRect, " ")))
	local graphW = math.floor(mainSeqWidth / 16 + 0.5)

	local knobSeqBGRect = getCompProp("uiSeq2BG", "componentRectangle")
	local knobSeqBGLeft = tonumber(string.sub(knobSeqBGRect, 1, string.find(knobSeqBGRect, " ")))
	local canvasH = mod:getHeight()

	local startX = math.abs(mainSeqWLeft - knobSeqBGLeft) - 2

	g:setColour(skinColors.darkText)
	g:setFont(Font(8.0, 1))

	for i = 1, 16 do

		-- Step numbers
		g:drawText(tostring(i), startX + (i - 1) * graphW, spacer, graphW, textH,
				 				     Justification(Justification.centred), false)

		g:drawText(tostring(i), startX + (i - 1) * graphW, canvasH - (textH + spacer), graphW, textH,
									 Justification(Justification.centred), false)
	end
end
```

---

## `externalRepaintSequencer`

```lua
function externalRepaintSequencer()
	
	getComp("uiSequencerScreen"):repaint()
end
```

---

## `sequencerMouseDown`

```lua
function sequencerMouseDown(comp, event)

	local barW = (comp:getWidth() / 16)
	local cBar = math.ceil(event.x / barW)

	-- Catch knob name and value to operate on it
	sharedValues.sequencerKnob = string.format("knobSeq%dStep%d", sharedValues.selectedSequence + 1, cBar)
	sharedValues.sequencerKnobValue = getModValue(sharedValues.sequencerKnob)

	sharedValues.sequencerStartY = event.y
	sharedValues.isSequencerDragging = true
end
```

---

## `sequencerMouseUp`

```lua
function sequencerMouseUp()

	sharedValues.isSequencerDragging = false
end
```

---

## `sequencerMouseDrag`

```lua
function sequencerMouseDrag(comp, event)

	if sharedValues.isSequencerDragging == true then

		local bounds
		local newValue
		local compH = comp:getHeight()
		local ratio, barRatio
		local deltaY = sharedValues.sequencerStartY - event.y
		local currKnobValue = getModValue(sharedValues.sequencerKnob)

		if sharedValues.timbreMode == tmSynth then
			bounds = getSeqStepMaxVal(modByName(string.format("cbSeqKnob%d", 
												sharedValues.selectedSequence + 1)):getValue())
		else
			bounds = getSeqStepMaxVal(sharedValues.selectedSequence)
		end

		if bounds[1] < 0 then
			barRatio = 1.25
		else
			barRatio = 1.125
		end

		ratio = (math.abs(bounds[1]) + math.abs(bounds[2])) / compH

		newValue = math.floor(sharedValues.sequencerKnobValue + (deltaY * ratio * barRatio))

		-- Set min or max values if they were exceeded
		if newValue < bounds[1] then
 			newValue = bounds[1]
 		elseif newValue > bounds[2] then
			newValue = bounds[2]
		end

		modByName(sharedValues.sequencerKnob):setModulatorValue(newValue, false, true, false)
	end
end
```

---

## `selectSequenceByValue`

```lua
function selectSequenceByValue(seqNumber)

	if sharedValues.selectedSequence ~= seqNumber then

		turnLightsOff("imgSeqLamp", 2)
		setLightState(string.format("imgSeqLamp%d", seqNumber), true)
		sharedValues.selectedSequence = seqNumber
	end

	-- Repaint SEQ graph since modulator value is changed
	getComp("uiSequencerScreen"):repaint()
end
```

---

## `sequencerMouseDblClick`

```lua
function sequencerMouseDblClick(comp, event)
	-- Resetting related knob to its default value by double click

	local barW = (comp:getWidth() / 16)
	local cBar = math.ceil(event.x / barW)

	local opKnob = string.format("knobSeq%dStep%d", sharedValues.selectedSequence + 1, cBar)

	setModValue(opKnob, getCompPropN(opKnob, "uiSliderDoubleClickValue"))
end
```

---

## `paintMidiActivity`

```lua
function paintMidiActivity(mod, g)
	
	g:setColour(skinColors.customBG)
	g:fillAll()

	if sharedValues.midiActivity == 1 then
		g:setColour(ICON_ORANGE)
	end

	g:fillEllipse(2, 7, 10, 10)
end
```

---

## `blinkMidiLight`

```lua
function blinkMidiLight()
	
	sharedValues.midiActivity = 1
	externalRepaintMidiActivity()
	blinkMidiLightTimer()
end
```

---

## `externalRepaintMidiActivity`

```lua
function externalRepaintMidiActivity()
	
	getComp("uiMidiActivity"):repaint()
end
```

---

## `applyVocoderLabels`

```lua
function applyVocoderLabels(vocoderSelected)

	local textColor
	local labelBG 
	local i

	local onlyColorChangeLabels = {
		"lblTimbreMidiCh",
		"lblTimbreAssign",
		"lblEG2Reset",
		"lblEG1Reset",
		"lblTimbreTrigger",
		"lblTimbrePriority",
		"lblTimbreDetune",
		"lblTimbreTune",
		"lblTimbreBendRange",
		"lblTimbreTranspose",
		"lblTimbreVibrato",
		"lblTimbrePorta",
		"lblAmpDistortion",
		"lblAmpVeloSens",
		"lblAmpKeyTrack",
		"lblLFO1KeySync",
		"lblLFO1TempoSync",
		"lblLFO1SyncNote",
		"lblLFO2KeySync",
		"lblLFO2TempoSync",
		"lblLFO2SyncNote",
		"lblOsc1Control1",
		"lblOsc1Control2",
		"lblOsc1WaveCycle",
		"lblMixerOsc1",
		"lblMixerNoise",
		"lblFilterCutoff",
		"lblFilterResonance",
		"lblEG1Attack",
		"lblEG2Attack",
		"lblEG1Decay",
		"lblEG2Decay",
		"lblEG1Sustain",
		"lblEG2Sustain",
		"lblEG1Release",
		"lblEG2Release",
		"lblAmpLevel",
		"lblLFO1TypeCycle",
		"lblLFO2TypeCycle",
		"lblLFO1Frequency",
		"lblLFO2Frequency",
		"lblArpOnOff",
		"lblArpLatch",
		"lblArpKeySync",
		"lblArpTempo",
		"lblArpGate",
		"lblArpResolution",
		"lblArpSwing",
		"lblArpTempo",
		"lblArpType",
		"lblArpRange"
	}

	--Change text and background for dedicated labels
	if vocoderSelected then

		-- Color depends on selected skin
		textColor = skinColors.vocoderText:toString()
		labelBG = skinColors.vocoderElements:toString()

		alterLableProps("lblFilter24LPF", "+ 1", labelBG, textColor)
		alterLableProps("lblFilter12LPF", "+ 2", labelBG, textColor)
		alterLableProps("lblFilter12BPF", "-  1", labelBG, textColor)
		alterLableProps("lblFilter12HPF", "-  2", labelBG, textColor)
		alterLableProps("lblOsc2Semitone", "HPF LEVEL", labelBG, textColor)
		alterLableProps("lblOsc2Tune", "THRESHOLD", labelBG, textColor)
		alterLableProps("lblMixerOsc2", "DIRECT", labelBG, textColor)
		alterLableProps("lblFilterTypeCycle", "FORMANT SHIFT", labelBG, textColor)
		alterLableProps("lblFilterKbdTrack", "E.F.SENSE", labelBG, textColor)
		alterLableProps("lblAmpPan", "DIRECT", labelBG, textColor)
		alterLableProps("lblSequence1", "LVL", labelBG, textColor)
		alterLableProps("lblSequence2", "PAN", labelBG, textColor)
		alterLableProps("lblFilterVeloSens", "GATE SENS", labelBG, textColor)
		alterLableProps("lblAmpEG2Gate", "HPF GATE", labelBG, textColor)
		alterLableProps("lblFilterEG1Int", "FC MOD INT", labelBG, textColor)
		alterLableProps("lblPatchSource1", "FC MOD SOURCE", labelBG, textColor)
	else

		textColor = skinColors.labelTextColor:toString()
		labelBG = COLOR_TRANSPARENT:toString()

		alterLableProps("lblFilter24LPF", "24LPF", labelBG, textColor)
		alterLableProps("lblFilter12LPF", "12LPF", labelBG, textColor)
		alterLableProps("lblFilter12BPF", "12BPF", labelBG, textColor)
		alterLableProps("lblFilter12HPF", "12HPF", labelBG, textColor)
		alterLableProps("lblOsc2Semitone", "SEMITONE", labelBG, textColor)
		alterLableProps("lblOsc2Tune", "TUNE", labelBG, textColor)
		alterLableProps("lblMixerOsc2", "OSC 2", labelBG, textColor)
		alterLableProps("lblFilterTypeCycle", "FILTER TYPE", labelBG, textColor)
		alterLableProps("lblFilterKbdTrack", "KBD TRK", labelBG, textColor)
		alterLableProps("lblAmpPan", "PAN", labelBG, textColor)
		alterLableProps("lblSequence1", "SEQ 1", labelBG, textColor)
		alterLableProps("lblSequence2", "SEQ 2", labelBG, textColor)
		alterLableProps("lblFilterVeloSens", "VELO SENS", labelBG, textColor)
		alterLableProps("lblAmpEG2Gate", "EG2 / GATE", labelBG, textColor)
		alterLableProps("lblFilterEG1Int", "EG 1 INT", labelBG, textColor)
		alterLableProps("lblPatchSource1", "SOURCE 1", labelBG, textColor)
	end

	for i = 1, #onlyColorChangeLabels do
		alterLabelColors(onlyColorChangeLabels[i], labelBG, textColor)
	end
end
```

---

## `paintMIDISettingsButton`

```lua
function paintMIDISettingsButton(mod, g)

	local canvasW = mod:getWidth()
	local canvasH = mod:getHeight()

	g:setColour(SEQ_BACKGROUND)
	g:fillRoundedRectangle(0, 0, canvasW, canvasH, 2)

	g:setColour(COLOR_VOCODER_LABEL)
	g:fillRoundedRectangle(4, 4, canvasW - 8, canvasH - 8, 1)

	g:setColour(SEQ_BACKGROUND)
	g:setFont(Font(13, 1))
	g:drawText("M", 5, 3, canvasW - 6, 14, Justification(Justification.left), false)
end
```

---

## `midiSettingsClick`

```lua
function midiSettingsClick()

	getComp("btnMidiDeviceDialogHidden"):click()
end
```

---

## `sendSysExMessage`

```lua
function sendSysExMessage(message)

	panel:sendMidiMessageNow(CtrlrMidiMessage(message))
end
```

---

## `inputMIDIParser`

```lua
function inputMIDIParser(midiMessage)

	blinkMidiLight()

	local msgData = midiMessage:getData()
	local msgSize = midiMessage:getSize()

	-- Usable data is longer than 3 bytes

	-- MIDI data
	if msgSize == 2 then

		-- Selected patch and bank
		if msgData:getByte(0) == (0xC0 + getGlobalMidiChannel(true))then
			captureProgramChangeMessage(msgData)
		end

	elseif msgSize == 3 then

		-- Incoming message from Program Play mode
		if msgData:getByte(0) == (0xB0 + getGlobalMidiChannel(true)) then

			processProgramPlayMessage(memBlockToTable(msgData))
		end

	-- SysEx parameter data
	elseif (msgSize >= 6) and (msgSize <= 15) then 

		local opType = msgData:getByte(4) 
		local opParam = msgData:getByte(5)

		local controlLS = msgData:getByte(5)
		local controlMS = msgData:getByte(6)

		-- Request replies may be caught here

		-- Identity reply
		if msgSize == 15 then
			if opType == 0x02 then
				if (msgData:getByte(5) == 0x42) and 
				   (msgData:getByte(6) == 0x58) and
				   (msgData:getByte(7) == 0x00) then

					setSynthReachStatus(dsOnline)
				end
			end
		end

		-- Exchange replies
		if msgSize == 6 then
			-- Write | Load successful reply
			if (opType == 0x21) or (opType == 0x23) then
				writeOKReply()
			end

			-- Write error reply
			if (opType == 0x22) or (opType == 0x24) then
				writeErrorReply()
			end
		end

		if msgSize == 8 then

			-- Mode data pushed from HW
			if opType == 0x4E then
				setOperationMode(opParam, true)
			end
		end

		-- Parameters transmission
		if msgSize == 10 then

			-- Requested mode data response
			if opType == 0x42 then
				setOperationMode(opParam, true)
			end

			-- Control change
			if opType == 0x41 then

				-- Since this panel use fancy "lamp radiobuttons" which aren't actual controllers, 
				-- incoming data handling code is required for them

				-- OSC1 waveform changed
				if	(controlLS == sharedValues.osc1SEValues[6]) and
					(controlMS == sharedValues.osc1SEValues[7])	then

					setOsc1WaveformByValue(msgData:getByte(7), true)
				end

				-- OSC2 waveform changed
				if	(controlLS == sharedValues.osc2SEValues[6]) and
 					(controlMS == sharedValues.osc2SEValues[7]) then

					setOsc2WaveformByValue(msgData:getByte(7), true)
				end

				-- OSC Modulation
				if	(controlLS == sharedValues.oscModSEValues[6]) and
 					(controlMS == sharedValues.oscModSEValues[7]) then

					processOscModData(msgData:getByte(7))
				end

				-- Filter type changed
				if	(controlLS == sharedValues.filterSEValues[6]) and
					(controlMS == sharedValues.filterSEValues[7]) then

					setFilterTypeByValue(msgData:getByte(7), true)
				end

				-- LFO1 waveform changed
				if	(controlLS == sharedValues.LFO1SEValues[6]) and
 					(controlMS == sharedValues.LFO1SEValues[7]) then

					setLFO1TypeByValue(msgData:getByte(7), true)
				end

				-- LFO2 waveform changed
				if	(controlLS == sharedValues.LFO2SEValues[6]) and
 					(controlMS == sharedValues.LFO2SEValues[7]) then

					setLFO2TypeByValue(msgData:getByte(7), true)
				end
			end
		end

	elseif  msgSize >= GLOBAL_DATA_SIZE then 

		local opType = msgData:getByte(4) 

		-- Current program data dump
		if (opType == 0x40) and (timerFlags.waitForSingleProgram == true) then
			captureProgramDumpData(msgData)
		end

		-- Patch bank data dump
		if (opType == 0x4C) and (timerFlags.waitForBulkDump == true) then
			captureBulkDumpData(msgData)
		end

		-- Global data dump
		if (opType == 0x51) and (timerFlags.waitForSettings == true) then
			captureGlobalSettings(msgData)
		end

		-- All data dump
		--if opType == 0x50 then
		--end
	end
end
```

---

## `getGlobalMidiChannel`

```lua
function getGlobalMidiChannel(pureValue)

	local mcByte = 0x30

	if pureValue ~= nil then
		if pureValue == true then
			mcByte = 0
		end
	end
		
	return mcByte + (panel:getPropertyInt("panelMidiOutputChannelDevice") - 1)
end
```

---

## `copySequenceToClipboard`

```lua
function copySequenceToClipboard()

	local i
	local startByte

	-- Copy sequence mode data into buffer
	if sharedValues.timbreMode == tmSynth then

		-- Sync data with buffer for using actual values
		syncTimbreWithBuffer(sharedValues.selectedTimbre)

		startByte = TIMBRE_ONE_STARTBYTE + SEQUENCE_STARTBYTE_DISP + (TIMBRE_DATA_SIZE * sharedValues.selectedTimbre)

		-- First byte - type of data being copied
		seqClipboard = {tmSynth}

		for i = startByte, startByte + SEQUENCE_DATA_SIZE do

			table.insert(seqClipboard, dataBuffer[i])
		end
	else

		syncVocoderWithBuffer()

		seqClipboard = {tmVocoder}

		for i = VOCODER_SEQDATA_STARTBYTE, VOCODER_SEQDATA_STARTBYTE + VOCODER_SEQDATA_SIZE - 1 do

			table.insert(seqClipboard, vocoderBuffer[i])
		end
	end

	showHint("Info: Sequence data copied to clipboard")

	repaintTimbreCPButtons()
end
```

---

## `pasteSequenceFromClipboard`

```lua
function pasteSequenceFromClipboard()

	local i
	local c = 2
	local startByte

	if #seqClipboard > 0 then

		-- First byte of clipboard - timbre type
		if seqClipboard[1] ~= sharedValues.timbreMode then

			genAlertWindow("Warning!", "Incompatible type of data. Vocoder data cannot be applied for synthesizer mode, and vice versa")
		else

			if not confirmDialog("Warning!", "This will erase corresponded data in the buffer. Proceed?") then
				return
			end

			if sharedValues.timbreMode == tmSynth then

				startByte = TIMBRE_ONE_STARTBYTE + SEQUENCE_STARTBYTE_DISP + (TIMBRE_DATA_SIZE * sharedValues.selectedTimbre)

				-- Apply clipboard data to dataBuffer
				for i = startByte, startByte + SEQUENCE_DATA_SIZE do

					-- Clipboard starts with 2nd byte. First is a clipboard type
					dataBuffer[i] = seqClipboard[c]

					c = c + 1
				end

				applyTimbreData(sharedValues.selectedTimbre, dataBuffer)
			else

				-- Apply clipboard to vocoderBuffer
				for i = VOCODER_SEQDATA_STARTBYTE, VOCODER_SEQDATA_STARTBYTE + VOCODER_SEQDATA_SIZE - 1 do

					vocoderBuffer[i] = seqClipboard[c]

					c = c + 1
				end

				-- Reapply the same voice mode for the changes to take effect
				applyVocoderData(vocoderBuffer)
			end

			-- Reset buffer and repaint buttons
			seqClipboard = {}
			repaintTimbreCPButtons()

			showHint("Info: Sequence data pasted from clipboard")
		end
	end
end
```

---

## `assertSeqKnobBounds`

```lua
function assertSeqKnobBounds(mod, selectedMode, source)
	
	if blockExecution(source) then
		return
	end

	if sharedValues.timbreMode == tmSynth then

		local knobGroup = getModPropN(mod, "modulatorCustomIndex") + 1
	
		assertSeqKnobBoundsByValue(knobGroup, selectedMode)
	end
end
```

---

## `getSeqStepMaxVal`

```lua
function getSeqStepMaxVal(selectedMode)

	local minV, maxV, defV

	if sharedValues.timbreMode == tmSynth then

		-- Synthesizer mode:

		-- As manual states:
		-- When Knob is "Step Length" (2)				00~7F : - 6~0~+ 6 (*2-6)
    	-- When Knob is "Pitch" or "OSC2 Semi" (1 or 6)	00~7F : -24~0~+24 (*2-7)
    	-- When Knob is others							00~7F : -63~0~+63 (*2-8)


		if selectedMode == 2 then
 			minV, maxV, defV = -6, 6, 0
		elseif selectedMode == 1 or selectedMode == 6 then
			minV, maxV, defV = -24, 24, 0
		else
			minV, maxV, defV = -63, 63, 0
		end
	else

		-- Vocoder mode:

		-- SEQ1 - Level [0~127]
		-- SEQ2 - Pan [-63/+63]

		if selectedMode == 0 then
			minV, maxV, defV = 0, 127, 64
		else
			minV, maxV, defV = -63, 63, 0
		end
	end

	return {minV, maxV, defV}
end
```

---

## `assertSysExFormulas`

```lua
function assertSysExFormulas(timbreNumber)

	-- Accepted values are:
	-- 0 - for Timbre mode, Timbre 1
	-- 1 - for Timbre mode, Timbre 2
	-- 2 - for Vocoder mode

	local i, j, startPoint
	local numShift
	local startPoint
	local lsms

	if (timbreNumber == 0) or (timbreNumber == 1) then

		numShift = timbreNumber * SYSEX_VAL_DIFF
		numShiftAlter = timbreNumber * SYSEX_VAL_DIFF_ALT

		-- Numbers were verified one-by-one

		-- Non-visual components
		sharedValues.osc1SEValues 		= setNewModNumber(sharedValues.osc1SEValues,	numShift + 0x49)
		sharedValues.oscModSEValues		= setNewModNumber(sharedValues.oscModSEValues,	numShift + 0x4E)
		sharedValues.osc2SEValues		= setNewModNumber(sharedValues.osc2SEValues,	numShift + 0x4D)
		sharedValues.filterSEValues		= setNewModNumber(sharedValues.filterSEValues,	numShift + 0x54)
		sharedValues.LFO1SEValues		= setNewModNumber(sharedValues.LFO1SEValues,	numShift + 0x68)
		sharedValues.LFO2SEValues		= setNewModNumber(sharedValues.LFO2SEValues,	numShift + 0x6D)

		-- Visual components
		-- Formulas in the synth mode can be calculated
		setSEFormulaMod("cbTimbreMidiCh", numShift + 0x43)
		setSEFormulaMod("cbTimbreAssign", numShift + 0x40)
		setSEFormulaMod("btnEG2Reset", numShift + 0x13D)
		setSEFormulaMod("btnEG1Reset", numShift + 0x13C)
		setSEFormulaMod("cbTimbreTrigger", numShift + 0x41)
		setSEFormulaMod("cbTimbrePriority", numShiftAlter + 0x7E)
		setSEFormulaMod("knobTimbreDetune", (numShift * 2) + 0x42)
		setSEFormulaMod("knobTimbreTune", numShift + 0x45)
		setSEFormulaMod("knobTimbreBendRange", numShift + 0x48)
		setSEFormulaMod("knobTimbreTranspose", numShift + 0x44)
		setSEFormulaMod("knobTimbreVibrato", numShift + 0x46)
		setSEFormulaMod("knobOsc1Control1", numShift + 0x4A)
		setSEFormulaMod("knobOsc2Semitone", numShift + 0x4F)
		setSEFormulaMod("knobOsc2Tune", numShift + 0x50)
		setSEFormulaMod("knobMixerOsc1", numShift + 0x51)
		setSEFormulaMod("knobMixerOsc2", numShift + 0x52)
		setSEFormulaMod("knobMixerNoise", numShift + 0x53)
		setSEFormulaMod("knobTimbrePorta", numShift + 0x47)
		setSEFormulaMod("knobFilterCutoff", numShift + 0x55)
		setSEFormulaMod("knobFilterResonance", numShift + 0x56)
		setSEFormulaMod("knobFilterEG1Int", numShift + 0x57)
		setSEFormulaMod("knobFilterVeloSens", numShift + 0x62)
		setSEFormulaMod("knobFilterKbdTrk", numShift + 0x58)
		setSEFormulaMod("knobAmpLevel", numShift + 0x59)
		setSEFormulaMod("knobAmpPan", numShift + 0x5A)
		setSEFormulaMod("btnAmpEG2Gate", numShift + 0x5B)
		setSEFormulaMod("btnAmpDistortion", numShift + 0x5C)
		setSEFormulaMod("knobEG1Attack", numShift + 0x5E)
		setSEFormulaMod("knobEG1Decay", numShift + 0x5F)
		setSEFormulaMod("knobEG1Sustain", numShift + 0x60)
		setSEFormulaMod("knobEG1Release", numShift + 0x61)
		setSEFormulaMod("knobEG2Attack", numShift + 0x63)
		setSEFormulaMod("knobEG2Decay", numShift + 0x64)
		setSEFormulaMod("knobEG2Sustain", numShift + 0x65)
		setSEFormulaMod("knobEG2Release", numShift + 0x66)
		setSEFormulaMod("knobAmpVeloSens", numShift + 0x67)
		setSEFormulaMod("knobAmpKeyTrack", numShift + 0x5D)
		setSEFormulaMod("knobLFO1Frequency", numShift + 0x69)
		setSEFormulaMod("cbLFO1KeySync", numShift + 0x6C)
		setSEFormulaMod("btnLFO1TempoSync", numShift + 0x6B)
		setSEFormulaMod("cbLFO1SyncNote", numShift + 0x6A)
		setSEFormulaMod("knobLFO2Frequency", numShift + 0x6E)
		setSEFormulaMod("cbLFO2KeySync", numShiftAlter + 0x71)
		setSEFormulaMod("btnLFO2TempoSync", numShiftAlter + 0x70)
		setSEFormulaMod("cbLFO2SyncNote", numShift + 0x6F)
		setSEFormulaMod("cbPatchSource1", numShiftAlter + 0x72)
		setSEFormulaMod("cbPatchSource2", numShiftAlter + 0x75)
		setSEFormulaMod("cbPatchSource3", numShiftAlter + 0x78)
		setSEFormulaMod("cbPatchSource4", numShiftAlter + 0x7B)
		setSEFormulaMod("cbPatchDestination1", numShiftAlter + 0x73)
		setSEFormulaMod("cbPatchDestination2", numShiftAlter + 0x76)
		setSEFormulaMod("cbPatchDestination3", numShiftAlter + 0x79)
		setSEFormulaMod("cbPatchDestination4", numShiftAlter + 0x7C)
		setSEFormulaMod("knobPatch1Amount", numShiftAlter + 0x74)
		setSEFormulaMod("knobPatch2Amount", numShiftAlter + 0x77)
		setSEFormulaMod("knobPatch3Amount", numShiftAlter + 0x7A)
		setSEFormulaMod("knobPatch4Amount", numShiftAlter + 0x7D)
		setSEFormulaMod("btnSeqOnOff", numShift + 0x100)
		setSEFormulaMod("cbSeqRunMode", numShift + 0x103)
		setSEFormulaMod("cbSeqResolution", numShift + 0x105)
		setSEFormulaMod("knobSeqLastStep", numShift + 0x101)
		setSEFormulaMod("cbSeqType", numShift + 0x102)
		setSEFormulaMod("cbSeqKeySync", numShift + 0x104)
		setSEFormulaMod("cbSeqKnob1", numShift + 0x106)
		setSEFormulaMod("cbSeqMotion1", numShift + 0x107)
		setSEFormulaMod("cbSeqKnob2", numShift + 0x118)
		setSEFormulaMod("cbSeqMotion2", numShift + 0x119)
		setSEFormulaMod("cbSeqKnob3", numShift + 0x12A)
		setSEFormulaMod("cbSeqMotion3", numShift + 0x12B)

		startPoint = 0x107
		for j = 1, 3 do
			for i = 1, 16 do
				setSEFormulaMod(string.format("knobSeq%dStep%d", j, i), numShift + startPoint + i + (18 * (j - 1)))
			end
		end
	else

		-- Non-visual components
		sharedValues.osc1SEValues 		= setNewModNumber(sharedValues.osc1SEValues, 0x270)
		sharedValues.filterSEValues		= setNewModNumber(sharedValues.filterSEValues, 0x300)
		sharedValues.LFO1SEValues		= setNewModNumber(sharedValues.LFO1SEValues, 0x318)
		sharedValues.LFO2SEValues		= setNewModNumber(sharedValues.LFO2SEValues, 0x320)

		setSEFormulaMod("cbTimbreMidiCh", 0x263)
		setSEFormulaMod("cbTimbreAssign", 0x260)
		setSEFormulaMod("btnEG2Reset", 0x267)
		setSEFormulaMod("btnEG1Reset", 0x266)
		setSEFormulaMod("cbTimbreTrigger", 0x261)
		setSEFormulaMod("cbTimbrePriority", 0x264)
		setSEFormulaMod("knobTimbreDetune", 0x262)
		setSEFormulaMod("knobTimbreTune", 0x269)
		setSEFormulaMod("knobTimbreBendRange", 0x26C)
		setSEFormulaMod("knobTimbreTranspose", 0x268)
		setSEFormulaMod("knobTimbreVibrato", 0x26A)
		setSEFormulaMod("knobTimbrePorta", 0x26B)
		setSEFormulaMod("knobOsc1Control1", 0x271)
		setSEFormulaMod("knobOsc2Semitone", 0x27B)
		setSEFormulaMod("knobOsc2Tune", 0x27D)
		setSEFormulaMod("knobMixerOsc1", 0x278)
		setSEFormulaMod("knobMixerOsc2", 0x279)
		setSEFormulaMod("knobMixerNoise", 0x27A)
		setSEFormulaMod("knobFilterCutoff", 0x301)
		setSEFormulaMod("knobFilterResonance", 0x302)
		setSEFormulaMod("knobFilterEG1Int", 0x304)
		setSEFormulaMod("knobFilterVeloSens", 0x27C)
		setSEFormulaMod("knobFilterKbdTrk", 0x305)
		setSEFormulaMod("knobAmpLevel", 0x308)
		setSEFormulaMod("knobAmpPan", 0x309)
		setSEFormulaMod("btnAmpEG2Gate", 0x27E)
		setSEFormulaMod("btnAmpDistortion", 0x30A)
		setSEFormulaMod("knobAmpVeloSens", 0x30B)
		setSEFormulaMod("knobAmpKeyTrack", 0x30C)
		setSEFormulaMod("knobEG1Attack", 0x314)
		setSEFormulaMod("knobEG1Decay", 0x315)
		setSEFormulaMod("knobEG1Sustain", 0x316)
		setSEFormulaMod("knobEG1Release", 0x317)
		setSEFormulaMod("knobEG2Attack", 0x310)
		setSEFormulaMod("knobEG2Decay", 0x311)
		setSEFormulaMod("knobEG2Sustain", 0x312)
		setSEFormulaMod("knobEG2Release", 0x313)
		setSEFormulaMod("knobLFO1Frequency", 0x319)
		setSEFormulaMod("cbLFO1KeySync", 0x31C)
		setSEFormulaMod("btnLFO1TempoSync", 0x31B)
		setSEFormulaMod("cbLFO1SyncNote", 0x31A)
		setSEFormulaMod("knobLFO2Frequency", 0x321)
		setSEFormulaMod("cbLFO2KeySync", 0x324)
		setSEFormulaMod("btnLFO2TempoSync", 0x323)
		setSEFormulaMod("cbLFO2SyncNote", 0x322)
		setSEFormulaMod("cbPatchSource1", 0x303)

		startPoint = 0x32F
		for j = 1, 2 do
			for i = 1, 16 do
				setSEFormulaMod(string.format("knobSeq%dStep%d", j, i), startPoint + i + (16 * (j - 1)))
			end
		end
	end

	-- Make new formulas work for input messages
	panel:getInputComparator():rehashComparator()
end
```

---

## `assertSeqKnobBoundsByValue`

```lua
function assertSeqKnobBoundsByValue(knobGroup, selectedMode)
	
	local i
	local minV, maxV, defV
	local currKnob

	local knobBounds = getSeqStepMaxVal(selectedMode)

	minV, maxV, defV = knobBounds[1], knobBounds[2], knobBounds[3]

	for i = 1, 16 do

		currKnob = string.format("knobSeq%dStep%d", knobGroup, i)

		setCompPropN(currKnob, "uiSliderMin", minV)
		setCompPropN(currKnob, "uiSliderMax", maxV)
		setCompPropN(currKnob, "uiSliderDoubleClickValue", defV)

		if modByName(currKnob):getValue() > maxV then
			modByName(currKnob):setModulatorValue(maxV, false, false, false)
		end

		if modByName(currKnob):getValue() < minV then
			modByName(currKnob):setModulatorValue(minV, false, false, false)
		end

		getComp(currKnob):repaint()
	end

	externalRepaintSequencer()
end
```

---

## `applyVocoderData`

```lua
function applyVocoderData(dataArray)

	mutePanelOut(true)

	-- Applying vocoder data to controllers
	local i
	local programData = copyTable(dataArray)

	-- Some cosmetics
	sharedValues.allowChangeSeq = false

	-- Vocoder buffer have no extra information
	local sb = 1

	-- Byte 0 - MIDI ch [-1, 0~15 = GLB, 1~16ch]
	setModValue("cbTimbreMidiCh", bit.band(programData[sb] + 1, 0x7F))

	-- Byte 1  
	-- Bit 6,7 - Assign Mode [0, 1, 2 = Mono, Poly, Unison]
	setModValue("cbTimbreAssign", extractPackByte(programData[sb + 1], 6, 7))

	-- Bit 5 - EG2 reset [0,1 = Off, On]
	setModValue("btnEG2Reset", extractPackByte(programData[sb + 1], 5))

	-- Bit 4 - EG1 reset [0,1 = Off, On]
	setModValue("btnEG1Reset", extractPackByte(programData[sb + 1], 4))

	-- Bit 3 - Trigger Mode [0,1 = Single, Multi] (use Mono/Unison Mode)
	setModValue("cbTimbreTrigger", extractPackByte(programData[sb + 1], 3))

	-- Bit 0~1 - Key Priority [0~2 = Last, Low, High]
	setModValue("cbTimbrePriority", extractPackByte(programData[sb + 1], 0, 1))

	-- Byte 2 - Unison Detune [0~99 = 0~99 cent] (use Unison Mode)
	setModValue("knobTimbreDetune", programData[sb + 2])

	-- PITCH

	-- Byte 3 - Tune [64+/-50 = 0+/-50 cent]
	setModValue("knobTimbreTune", programData[sb + 3] - 64)

	-- Byte 4 - Bend Range [64+/-12 = 0+/-12 note]
	setModValue("knobTimbreBendRange", programData[sb + 4] - 64)

	-- Byte 5 - Transpose [64+/-24 = 0+/-24 note]
	setModValue("knobTimbreTranspose", programData[sb + 5] - 64)

	-- Byte 6 - Vibrato Int [64+/-63 = 0+/-63]
	setModValue("knobTimbreVibrato", programData[sb + 6] - 64)

	-- OSC

	-- Byte 7 - Wave [0~7 = Saw~Audio In]	
	setOsc1WaveformByValue(programData[sb + 7], true)

	-- Byte 8 - Waveform CTRL1 [0~127]
	setModValue("knobOsc1Control1", programData[sb + 8])

	if programData[sb + 7] ~= 5 then -- DWGS not selected

		-- Byte 9 - Waveform CTRL2 [0~127]
		setModValue("knobOsc1Control2", programData[sb + 9])
	else
 
		-- Byte 10 - DWGS Wave [0~63 = DWGS No. 1~64 (when OSC1 Wave is "DWGS")]
		setModValue("knobOsc1Control2", programData[sb + 10])
	end

	-- Byte 11 - (dummy byte)

	-- AUDIO IN2

	-- Byte 12
	-- Bit 1~7 - not use
	-- Bit 0 - HPF Gate [0, 1 = Dis, Ena]
	setModValue("btnAmpEG2Gate", extractPackByte(programData[sb + 12], 0))

	-- Byte 13 - (dummy byte)

	-- PITCH (2)

	-- Byte 14
	-- Bit 7 - not use [(0)
	-- Bit 0~6 - Portamento Time [0~127]
	setModValue("knobTimbrePorta", extractPackByte(programData[sb + 14], 0, 6))

	-- MIXER

	-- Byte 15 - OSC1 Level [0~127]
	setModValue("knobMixerOsc1", programData[sb + 15])

	-- Byte 16 - Ext1 Level [0~127]
	setModValue("knobMixerOsc2", programData[sb + 16])

	-- Byte 17 - Noise Level [0~127]
	setModValue("knobMixerNoise", programData[sb + 17])

	-- AUDIO IN2 (2)

	-- Byte 18 - HPF Level [0~127]
	setModValue("knobOsc2Semitone", programData[sb + 18])

	-- Byte 19 - Gate Sense [0~127]
	setModValue("knobFilterVeloSens", programData[sb + 19])

	-- Byte 20 - Threshold [0~127]
	setModValue("knobOsc2Tune", programData[sb + 20])

	-- FILTER

	-- Byte 21 - Shift [0~4 = 0, +1, +2, -1, -2]
	setFilterTypeByValue(programData[sb + 21], true)

	-- Byte 22 - Cutoff [64+/-63 = 0+/-63]
	setModValue("knobFilterCutoff", programData[sb + 22] - 64)

	-- Byte 23 - Resonance [0~127]
	setModValue("knobFilterResonance", programData[sb + 23])

	-- Byte 24 - Mod Source [0~7 = EG1~MIDI2]
	setModValue("cbPatchSource1", programData[sb + 24])

	-- Byte 25 - Intensity [64+/-63 = 0+/-63]
	setModValue("knobFilterEG1Int", programData[sb + 25] - 64)

	-- Byte 26 - E.F.Sense [0~127]
	setModValue("knobFilterKbdTrk", programData[sb + 26])

	-- AMP

	-- Byte 27 - Level [0~127]
	setModValue("knobAmpLevel", programData[sb + 27])

	-- Byte 28 - Direct Level [0~127]
	setModValue("knobAmpPan", programData[sb + 28])

	-- Byte 29
	-- Bit 1~7 - not use
	-- Bit 0 - Distortion On/Off [0, 1 = Off, On]
	setModValue("btnAmpDistortion", extractPackByte(programData[sb + 29], 0))

	-- Byte 30 - Vel.Sense [64+/-63 = 0+/-63]
	setModValue("knobAmpVeloSens", programData[sb + 30] - 64)

	-- Byte 31 - KeyTrack [64+/-63 = 0+/-63]
	setModValue("knobAmpKeyTrack", programData[sb + 31] - 64)

	-- EG1

	-- Byte 32 - Attack [0~127]
	setModValue("knobEG1Attack", programData[sb + 32])

	-- Byte 33 - Decay [0~127]
	setModValue("knobEG1Decay", programData[sb + 33])

	-- Byte 34 - Sustain [0~127]
	setModValue("knobEG1Sustain", programData[sb + 34])

	-- Byte 35 - Release [0~127]
	setModValue("knobEG1Release", programData[sb + 35])

	-- EG2

	-- Byte 36 - Attack [0~127]
	setModValue("knobEG2Attack", programData[sb + 36])

	-- Byte 37 - Decay [0~127]
	setModValue("knobEG2Decay", programData[sb + 37])

	-- Byte 38 - Sustain [0~127]
	setModValue("knobEG2Sustain", programData[sb + 38])

	-- Byte 39 - Release [0~127]
	setModValue("knobEG2Release", programData[sb + 39])

	-- LFO1

	-- Byte 40
	-- Bit 6,7 - not use
	-- Bit 4,5 - Key Sync [0~2 = OFF, Timbre, Voice]
	setModValue("cbLFO1KeySync", extractPackByte(programData[sb + 40], 4, 5))

	-- Bit 2,3 - not use
	-- Bit 0,1 - Wave [0~3 = Saw, Squ, Tri, S/H]
	setLFO1TypeByValue(extractPackByte(programData[sb + 40], 0, 1), true)

	-- Byte 41 - Frequency [0~127]
	setModValue("knobLFO1Frequency", programData[sb + 41])

	-- Byte 42
	-- Bit 7 - Tempo Sync [0,1 = Off,On]
	setModValue("btnLFO1TempoSync", extractPackByte(programData[sb + 42], 7))

	-- Bit 5,6 - not use
	-- Bit 0~4 - Sync Note [0~14 = 1/1~1/32]
	setModValue("cbLFO1SyncNote", extractPackByte(programData[sb + 42], 0, 4))

	-- LFO2

	-- Byte 43
	-- Bit 6,7 - not use
	-- Bit 4,5 - Key Sync [0~2 = OFF, Timbre, Voice]
	setModValue("cbLFO2KeySync", extractPackByte(programData[sb + 43], 4, 5))

	-- Bit 2,3 - not use
	-- Bit 0,1 - Wave [0~3 = Saw, Squ(+), Sin, S/H]
	setLFO2TypeByValue(extractPackByte(programData[sb + 43], 0, 1), true)

	-- Byte 44 - Frequency [0~127]
	setModValue("knobLFO2Frequency", programData[sb + 44])

	-- Byte 45
	-- Bit 7 - Tempo Sync [0, 1 = Off, On]
	setModValue("btnLFO2TempoSync", extractPackByte(programData[sb + 45], 7))

	-- Bit 5,6 - not use
	-- Bit 0~4 - Sync Note [0~14 = 1/1~1/32]
	setModValue("cbLFO2SyncNote", extractPackByte(programData[sb + 45], 0, 4))

	-- CH LEVEL [0]~[15] = CH[1]~[16]

	-- Byte 46~61 - Level [0~15] - 0~127
	for i = 1, 16 do
		setModValue(string.format("knobSeq1Step%d", i), programData[sb + 45 + i])
	end

	-- CH PAN  [0]~[15] = CH[1]~[16]

	-- Byte 62~77 - Pan  [0~15] - 1~64~127 = L63~CNT~R63
	for i = 1, 16 do
		setModValue(string.format("knobSeq2Step%d", i), programData[sb + 61 + i] - 64)
	end

	-- Select SEQ1 to show
	selectSequenceByValue(0)

	mutePanelOut(false)
end
```

---

## `syncVocoderWithBuffer`

```lua
function syncVocoderWithBuffer()

	-- Applying vocoder data with buffer
	local sb
	local i

	sb = 1

	-- Byte 0 - MIDI ch [-1, 0~15 = GLB, 1~16ch]
	vocoderBuffer[sb] = bit.band(getModValue("cbTimbreMidiCh") - 1, 0xFF)

	-- Byte 1  
	-- Bit 6,7 - Assign Mode [0, 1, 2 = Mono, Poly, Unison]
	vocoderBuffer[sb + 1] = packBitsToByte(vocoderBuffer[sb + 1], tonumber(getModValue("cbTimbreAssign")), 6, 7)

	-- Bit 5 - EG2 reset [0, 1 = Off, On]
	vocoderBuffer[sb + 1] = packBitsToByte(vocoderBuffer[sb + 1], getModValue("btnEG2Reset"), 5)

	-- Bit 4 - EG1 reset [0, 1 = Off, On]
	vocoderBuffer[sb + 1] = packBitsToByte(vocoderBuffer[sb + 1], getModValue("btnEG1Reset"), 4)

	-- Bit 3 - Trigger Mode [0,1=Single,Multi] (use Mono/Unison Mode)
	vocoderBuffer[sb + 1] = packBitsToByte(vocoderBuffer[sb + 1], getModValue("cbTimbreTrigger"), 3)

	-- Bit 0~1 - Key Priority [0~2 = Last, Low, High]
	vocoderBuffer[sb + 1] = packBitsToByte(vocoderBuffer[sb + 1], getModValue("cbTimbrePriority"), 0, 1)

	-- Byte 2 - Unison Detune [0~99 = 0~99 cent] (use Unison Mode)
	vocoderBuffer[sb + 2] = getModValue("knobTimbreDetune")

	-- PITCH

	-- Byte 3 - Tune [64+/-50 = 0+/-50[cent]
	vocoderBuffer[sb + 3] = getModValue("knobTimbreTune") + 64

	-- Byte 4 - Bend Range [64+/-12 = 0+/-12[note]
	vocoderBuffer[sb + 4] = getModValue("knobTimbreBendRange") + 64

	-- Byte 5 - Transpose [64+/-24 = 0+/-24[note]
	vocoderBuffer[sb + 5] = getModValue("knobTimbreTranspose") + 64

	-- Byte 6 - Vibrato Int [64+/-63 = 0+/-63
	vocoderBuffer[sb + 6] = getModValue("knobTimbreVibrato") + 64

	-- OSC

	-- Byte 7 - Wave [0~7 = Saw~Audio In]
	-- Stored directly in buffer, not in modulator, so no reason to sync it

	-- Byte 8 - Waveform CTRL1 [0~127]
	vocoderBuffer[sb + 8] = getModValue("knobOsc1Control1")

	if vocoderBuffer[sb + 7] ~= 5 then -- DWGS not selected

		-- Byte 9 - Waveform CTRL2 [0~127]
		vocoderBuffer[sb + 9] = getModValue("knobOsc1Control2")
	else
 
		-- Byte 10 - DWGS Wave [0~63 = DWGS No. 1~64 (when OSC1 Wave is "DWGS")]
		vocoderBuffer[sb + 10] = getModValue("knobOsc1Control2")
	end

	-- Byte 11 - (dummy byte)

	-- AUDIO IN2

	-- Byte 12
	-- Bit 1~7 - not use
	-- Bit 0 - HPF Gate [0, 1 = Dis, Ena]
	vocoderBuffer[sb + 12] = packBitsToByte(vocoderBuffer[sb + 12], getModValue("btnAmpEG2Gate"), 0, 1)

	-- Byte 13 - (dummy byte)

	-- PITCH (2)

	-- Byte 14
	-- Bit 7 - not use [(0)
	-- Bit 0~6 - Portamento Time [0~127]
	vocoderBuffer[sb + 14] = packBitsToByte(vocoderBuffer[sb + 14], getModValue("knobTimbrePorta"), 0, 6)

	-- MIXER

	-- Byte 15 - OSC1 Level [0~127]
	vocoderBuffer[sb + 15] = getModValue("knobMixerOsc1")

	-- Byte 16 - Ext1 Level [0~127]
	vocoderBuffer[sb + 16] = getModValue("knobMixerOsc2")

	-- Byte 17 - Noise Level [0~127]
	vocoderBuffer[sb + 17] = getModValue("knobMixerNoise")

	-- AUDIO IN2 (2)

	-- Byte 18 - HPF Level [0~127]
	vocoderBuffer[sb + 18] = getModValue("knobOsc2Semitone")

	-- Byte 19 - Gate Sense [0~127]
	vocoderBuffer[sb + 19] = getModValue("knobFilterVeloSens")

	-- Byte 20 - Threshold [0~127]
	vocoderBuffer[sb + 20] = getModValue("knobOsc2Tune")

	-- FILTER

	-- Byte 21 - Shift [0~4 = 0, +1, +2, -1, -2]
	-- Stored directly in buffer

	-- Byte 22 - Cutoff [64+/-63 = 0+/-63]
	vocoderBuffer[sb + 22] = getModValue("knobFilterCutoff") + 64

	-- Byte 23 - Resonance [0~127]
	vocoderBuffer[sb + 23] = getModValue("knobFilterResonance")

	-- Byte 24 - Mod Source [0~7 = EG1~MIDI2]
	vocoderBuffer[sb + 24] = getModValue("cbPatchSource1")

	-- Byte 25 - Intensity [64+/-63 = 0+/-63]
	vocoderBuffer[sb + 25] = getModValue("knobFilterEG1Int") + 64

	-- Byte 26 - E.F.Sense [0~127]
	vocoderBuffer[sb + 26] = getModValue("knobFilterKbdTrk")

	-- AMP

	-- Byte 27 - Level [0~127]
	vocoderBuffer[sb + 27] = getModValue("knobAmpLevel")

	-- Byte 28 - Direct Level [0~127]
	vocoderBuffer[sb + 28] = getModValue("knobAmpPan")

	-- Byte 29
	-- Bit 1~7 - not use
	-- Bit 0 - Distortion On/Off [0, 1 = Off, On]
	vocoderBuffer[sb + 29] = packBitsToByte(vocoderBuffer[sb + 29], getModValue("btnAmpDistortion"), 0)

	-- Byte 30 - Vel.Sense [64+/-63 = 0+/-63]
	vocoderBuffer[sb + 30] = getModValue("knobAmpVeloSens") + 64

	-- Byte 31 - KeyTrack [64+/-63 = 0+/-63]
	vocoderBuffer[sb + 31] = getModValue("knobAmpKeyTrack") + 64

	-- EG1

	-- Byte 32 - Attack [0~127]
	vocoderBuffer[sb + 32] = getModValue("knobEG1Attack")

	-- Byte 33 - Decay [0~127]
	vocoderBuffer[sb + 33] = getModValue("knobEG1Decay")

	-- Byte 34 - Sustain [0~127]
	vocoderBuffer[sb + 34] = getModValue("knobEG1Sustain")

	-- Byte 35 - Release [0~127]
	vocoderBuffer[sb + 35] = getModValue("knobEG1Release")

	-- EG2

	-- Byte 36 - Attack [0~127]
	vocoderBuffer[sb + 36] = getModValue("knobEG2Attack")

	-- Byte 37 - Decay [0~127]
	vocoderBuffer[sb + 37] = getModValue("knobEG2Decay")

	-- Byte 38 - Sustain [0~127]
	vocoderBuffer[sb + 38] = getModValue("knobEG2Sustain")

	-- Byte 39 - Release [0~127]
	vocoderBuffer[sb + 39] = getModValue("knobEG2Release")

	-- LFO1

	-- Byte 40
	-- Bit 6,7 - not use
	-- Bit 4,5 - Key Sync [0~2 = OFF, Timbre, Voice]
	vocoderBuffer[sb + 40] = packBitsToByte(vocoderBuffer[sb + 40], getModValue("cbLFO1KeySync"), 4, 5)

	-- Bit 2,3 - not use
	-- Bit 0,1 - Wave [0~3 = Saw, Squ, Tri, S/H]
	-- Stored directly in buffer

	-- Byte 41 - Frequency [0~127]
	vocoderBuffer[sb + 41] = getModValue("knobLFO1Frequency")

	-- Byte 42
	-- Bit 7 - Tempo Sync [0,1 = Off,On]
	vocoderBuffer[sb + 42] = packBitsToByte(vocoderBuffer[sb + 42], getModValue("btnLFO1TempoSync"), 7)

	-- Bit 5,6 - not use
	-- Bit 0~4 - Sync Note [0~14 = 1/1~1/32]
	vocoderBuffer[sb + 42] = packBitsToByte(vocoderBuffer[sb + 42], getModValue("cbLFO1SyncNote"), 0, 4)

	-- LFO2

	-- Byte 43
	-- Bit 6,7 - not use
	-- Bit 4,5 - Key Sync [0~2 = OFF, Timbre, Voice]
	vocoderBuffer[sb + 43] = packBitsToByte(vocoderBuffer[sb + 43], getModValue("cbLFO2KeySync"), 4, 5)

	-- Bit 2,3 - not use
	-- Bit 0,1 - Wave [0~3 = Saw, Squ(+), Sin, S/H]
	-- Stored directly in buffer

	-- Byte 44 - Frequency [0~127]
	vocoderBuffer[sb + 44] = getModValue("knobLFO2Frequency")

	-- Byte 45
	-- Bit 7 - Tempo Sync [0, 1 = Off, On]
	vocoderBuffer[sb + 45] = packBitsToByte(vocoderBuffer[sb + 45], getModValue("btnLFO2TempoSync"), 7)

	-- Bit 5,6 - not use
	-- Bit 0~4 - Sync Note [0~14 = 1/1~1/32]
	vocoderBuffer[sb + 45] = packBitsToByte(vocoderBuffer[sb + 45], getModValue("cbLFO2SyncNote"), 0, 4)

	-- CH LEVEL [0]~[15] = CH[1]~[16]

	-- Byte 46~61 - Level [0~15] - 0~127
	for i = 1, 16 do
		vocoderBuffer[sb + 45 + i] = getModValue(string.format("knobSeq1Step%d", i))
	end

	-- CH PAN  [0]~[15] = CH[1]~[16]

	-- Byte 62~77 - Pan  [0~15] - 1~64~127 = L63~CNT~R63
	for i = 1, 16 do
		vocoderBuffer[sb + 61 + i] = getModValue(string.format("knobSeq2Step%d", i)) + 64
	end

end
```

---

## `setVocoderMode`

```lua
function setVocoderMode(vocoderEnabled)

	local i
	local knobList = {
		"knobOsc2Semitone",
		"knobOsc2Tune", 
		"knobFilterKbdTrk", 
		"knobAmpPan", 
		"knobFilterVeloSens"
	}

	local modLIst = {
		"btnOsc2OscModCycle",
		"btnOsc2WaveCycle",
		"imgOsc2Lamp0",
		"imgOsc2Lamp1",
		"imgOsc2Lamp2",
		"imgOsc2ModLamp0",
		"imgOsc2ModLamp1",
		"cbPatchDestination1",
		"cbPatchDestination2",
		"cbPatchDestination3",
		"cbPatchDestination4",
		"cbPatchSource2",
		"cbPatchSource3",
		"cbPatchSource4",
		"knobPatch1Amount",
		"knobPatch2Amount",
		"knobPatch3Amount",
		"knobPatch4Amount",
		"knobSeqLastStep",
		"cbSeqKnob1",
		"cbSeqKnob2",
		"cbSeqKnob3",
		"cbSeqMotion1",
		"cbSeqMotion2",
		"cbSeqMotion3",
		"cbSeqType",
		"cbSeqRunMode",
		"cbSeqKeySync",
		"cbSeqResolution",
		"btnSeqOnOff",
		"imgSeqLamp2"
	}

	for i = 1, 16 do
		table.insert(modLIst, string.format("knobSeq3Step%d", i))
	end

	-- Vocoder used to have these fancy "inverted color" labels
	applyVocoderLabels(vocoderEnabled)

	if vocoderEnabled then

		-- Synchronizing values

		if sharedValues.voiceMode ~= vmUndefined then

			syncTimbreWithBuffer(sharedValues.selectedTimbre)
		else

			-- Reset all "synth"-related values to its defaults
			resetVocoderControls(true)
		end

		turnLightsOff("imgOsc2ModLamp", 1)
		turnLightsOff("imgOsc2Lamp", 2)

		sharedValues.timbreMode = tmVocoder

		-- Change bounds
		for i = 1, #knobList do

			getComp(knobList[i]):setPropertyInt("uiSliderMin", 0)
			getComp(knobList[i]):setPropertyInt("uiSliderMax", 127)
			getComp(knobList[i]):setPropertyInt("uiSliderDoubleClickValue", 64)

			getComp(knobList[i]):repaint()
		end

		-- The "Cutoff" knob is invertd here - 0~127 => -63~63
 		specialBounds("knobFilterCutoff", -63, 63, 0)

		assertSeqKnobBoundsByValue(1, 0)
		assertSeqKnobBoundsByValue(2, 1)

		-- Set new formulas
		assertSysExFormulas(2)

		-- Disable some controls
		enableControls(modLIst, false)
	else
		-- Synchronizing values
		if sharedValues.voiceMode == vmVocoder then
			syncVocoderWithBuffer()
		end

		sharedValues.timbreMode = tmSynth

		-- Revert bounds
		for i = 1, #knobList do

			if knobList[i] ~= "knobOsc2Semitone" then

				getComp(knobList[i]):setPropertyInt("uiSliderMin", -63)
				getComp(knobList[i]):setPropertyInt("uiSliderMax", 63)
				getComp(knobList[i]):setPropertyInt("uiSliderDoubleClickValue", 0)
			else

				getComp(knobList[i]):setPropertyInt("uiSliderMin", -24)
				getComp(knobList[i]):setPropertyInt("uiSliderMax", 24)
				getComp(knobList[i]):setPropertyInt("uiSliderDoubleClickValue", 0)
			end

			getComp(knobList[i]):repaint()
		end

		-- The "Cutoff" knob again requires some special treatment
		specialBounds("knobFilterCutoff", 0, 127, 100)

		-- Set usual formulas
		assertSysExFormulas(0)

		-- Enable controls
		enableControls(modLIst, true)
	end

end
```

---

## `initPatchRawData`

```lua
function initPatchRawData()
	
	-- Raw patch data which should be converted to make it usable
	-- Mostly for testing and reference purposes

	-- INIT Patch
	return {
	-- SysEx Header
	0xF0, 0x42, 0x30, 0x58, 0x40, 
 	-- Program data begin
	--MS    B1    B2    B3    B4    B5    B6    B7   
	0x00, 0x49, 0x4E, 0x49, 0x54, 0x20, 0x50, 0x72, 
	0x00, 0x6F, 0x67, 0x72, 0x61, 0x6D, 0x00, 0x0B, 
	0x00, 0x00, 0x00, 0x40, 0x00, 0x3C, 0x00, 0x28, 
	0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x14, 0x40, 
	0x00, 0x0F, 0x40, 0x00, 0x78, 0x00, 0x00, 0x50, 
	0x08, 0x01, 0x00, 0x00, 0x7F, 0x70, 0x0A, 0x40, 
	0x00, 0x42, 0x40, 0x45, 0x00, 0x00, 0x00, 0x00,  
	0x00, 0x00, 0x00, 0x40, 0x40, 0x00, 0x7F, 0x00, 
	0x00, 0x00, 0x01, 0x7F, 0x0A, 0x40, 0x40, 0x40, 
	0x00, 0x7F, 0x40, 0x00, 0x40, 0x40, 0x00, 0x40, 
	0x00, 0x7F, 0x00, 0x00, 0x40, 0x7F, 0x00, 0x02, 
	0x00, 0x0A, 0x03, 0x02, 0x46, 0x0C, 0x02, 0x40, 
	0x00, 0x03, 0x40, 0x42, 0x40, 0x43, 0x40, 0x43, 
	0x01, 0x71, 0x01, 0x01, 0x40, 0x40, 0x40, 0x40, 
	0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
	0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x01, 
	0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
	0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
	0x00, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40, 0x40, 
	0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7F, 
	0x00, 0x70, 0x0A, 0x40, 0x42, 0x40, 0x45, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 
	0x00, 0x00, 0x7F, 0x00, 0x00, 0x01, 0x7F, 0x0A, 
	0x00, 0x40, 0x40, 0x40, 0x7F, 0x40, 0x00, 0x40, 
	0x00, 0x40, 0x00, 0x40, 0x7F, 0x00, 0x00, 0x40, 
	0x00, 0x7F, 0x00, 0x02, 0x0A, 0x03, 0x02, 0x46, 
	0x00, 0x0C, 0x02, 0x40, 0x03, 0x40, 0x42, 0x40, 
	0x08, 0x43, 0x40, 0x43, 0x71, 0x01, 0x01, 0x40, 
	0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
	0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
	0x00, 0x40, 0x00, 0x01, 0x40, 0x40, 0x40, 0x40, 
	0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
	0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x01, 
	0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
	0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
	0x00, 0x40, 0x40, 
	-- Program data end
	0xF7
	}
end
```

---

## `initPatchData`

```lua
function initPatchData()
	
	-- This data is good to work with, but it should
	-- be converted before sending to the synth

	-- INIT Patch
	return {
	-- SysEx Header
	0xF0, 0x42, 0x30, 0x58, 0x40,
	-- Program data begin
	--B1    B2    B3    B4    B5    B6    B7 
	0x49, 0x4E, 0x49, 0x54, 0x20, 0x50, 0x72,
	0x6F, 0x67, 0x72, 0x61, 0x6D, 0x00, 0x00,
	0x00, 0x00, 0x40, 0x00, 0x3C, 0x05, 0x28,
	0x00, 0x00, 0x14, 0x00, 0x00, 0x14, 0x40,
	0x0F, 0x40, 0x00, 0x78, 0x00, 0x00, 0x50,
	0x01, 0x00, 0x00, 0xFF, 0x70, 0x0A, 0x40,
	0x42, 0x40, 0x45, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x40, 0x40, 0x00, 0x7F, 0x00,
	0x00, 0x01, 0x7F, 0x14, 0x40, 0x40, 0x40,
	0x7F, 0x40, 0x00, 0x40, 0x40, 0x00, 0x40,
	0x7F, 0x00, 0x00, 0x40, 0x7F, 0x00, 0x02,
	0x0A, 0x03, 0x02, 0x46, 0x0C, 0x02, 0x40,
	0x03, 0x40, 0x42, 0x40, 0x43, 0x40, 0x43,
	0xF1, 0x01, 0x01, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x01,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x00, 0x01, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0xFF,
	0x70, 0x0A, 0x40, 0x42, 0x40, 0x45, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40,
	0x00, 0x7F, 0x00, 0x00, 0x01, 0x7F, 0x14,
	0x40, 0x40, 0x40, 0x7F, 0x40, 0x00, 0x40,
	0x40, 0x00, 0x40, 0x7F, 0x00, 0x00, 0x40,
	0x7F, 0x00, 0x02, 0x0A, 0x03, 0x02, 0x46,
	0x0C, 0x02, 0x40, 0x03, 0x40, 0x42, 0x40,
	0x43, 0x40, 0x43, 0xF1, 0x01, 0x01, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x00, 0x01, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x01,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 
	-- Program data end
	0xF7
	}
end
```

---

## `initPresetBank`

```lua
function initPresetBank()
	
	local progBankStorage = {}
	local progBank = {}
	local i, j

	for i = 1, 8 do

		progBank = {}
		for j = 1, 16 do

			table.insert(progBank, initPatchData())
		end

		table.insert(progBankStorage, progBank)
	end

	return progBankStorage
end
```

---

## `initVocoderBuffer`

```lua
function initVocoderBuffer()

	-- Vocoder buffer init data
	return {
	0xFF, 0x70, 0x0A, 0x40, 0x42, 0x40, 0x45, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x7F, 0x00, 0x00, 0x00, 0x64, 0x00, 
	0x00, 0x40, 0x14, 0x02, 0x40, 0x1E, 0x7F, 
	0x00, 0x00, 0x40, 0x40, 0x00, 0x40, 0x7F, 
	0x00, 0x00, 0x40, 0x7F, 0x00, 0x02, 0x0A, 
	0x03, 0x02, 0x46, 0x0C, 0x7F, 0x7F, 0x7F, 
	0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 
	0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x40, 
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
	0x40, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 
	0x00, 0x00, 0x00, 0x40, 0x00, 0x3C, 0x05, 
	0x28, 0x00, 0x00, 0x14, 0x00, 0x00, 0x14, 
	0x40, 0x0F, 0x40, 0x00, 0x78, 0x00, 0x00, 
	0x50, 0x01, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	}
end
```

---

## `resetVocoderControls`

```lua
function resetVocoderControls()

	local i

	for i = 1, 16 do
		modByName(string.format("knobSeq3Step%d", i)):setValue(0, false)
	end

	setModValue("knobSeqLastStep", 16)
	setModValue("cbSeqKnob1", 0)
	setModValue("cbSeqKnob2", 0)
	setModValue("cbSeqKnob3", 0)
	setModValue("cbSeqMotion1", 1)
	setModValue("cbSeqMotion1", 1)
	setModValue("cbSeqMotion1", 1)
	setModValue("cbSeqType", 0)
	setModValue("cbSeqRunMode", 1)
	setModValue("cbSeqKeySync", 1)
	setModValue("cbSeqResolution", 3)
	setModValue("btnSeqOnOff", 0)
	setModValue("cbArpTarget", 2)
	setModValue("cbPatchDestination1", 0)
	setModValue("cbPatchDestination2", 0)
	setModValue("cbPatchDestination3", 4)
	setModValue("cbPatchDestination4", 4)
	setModValue("cbPatchSource2", 3)
	setModValue("cbPatchSource2", 2)
	setModValue("cbPatchSource2", 3)
	setModValue("knobPatch2Amount", 0)
	setModValue("knobPatch3Amount", 0)
	setModValue("knobPatch4Amount", 0)
	setModValue("knobFilterEG1Int", 0)
end
```

---

## `initBufferWithInitPatch`

```lua
function initBufferWithInitPatch()

	dataBuffer = initPatchData()
	applyProgramData(dataBuffer, nil, nil, false)
end
```

---

## `cycleBanks`

```lua
function cycleBanks(mod, value, source)
	
	if blockExecution(source) then
		return
	end

	local cycleDirection = getModPropN(mod, "modulatorCustomIndex")

	-- Choose, where selection must happen
	if panelSettings.selectorsSource == pbsPanel then

		if cycleDirection == 1 then
			cycleBankValues(true, pbsPanel)
		else
			cycleBankValues(false, pbsPanel)
		end

		chosenPresetToBuffer((sharedValues.selectedBank * 100) + sharedValues.selectedPreset, false)
	else

		if cycleDirection == 1 then
			cycleBankValues(true, pbsSynth)
		else
			cycleBankValues(false, pbsSynth)
		end

		selectPresetOnSynth()
	end
end
```

---

## `cycleBankValues`

```lua
function cycleBankValues(increase, selectionDest)

	if selectionDest == pbsPanel then

		if increase then
			sharedValues.selectedBank = sharedValues.selectedBank + 1

			if sharedValues.selectedBank > 8 then
				sharedValues.selectedBank = 1
			end
		else

			sharedValues.selectedBank = sharedValues.selectedBank - 1

			if sharedValues.selectedBank < 1 then
				sharedValues.selectedBank = 8
			end
		end
	else

		if increase then
			sharedValues.synthBank = sharedValues.synthBank + 1

			if sharedValues.synthBank > 7 then
				sharedValues.synthBank = 0
			end
		else

			sharedValues.synthBank = sharedValues.synthBank - 1

			if sharedValues.synthBank < 0 then
				sharedValues.synthBank = 7
			end
		end
	end
end
```

---

## `openProgramBankFile`

```lua
function openProgramBankFile()

	-- File open dialog
	local bulkDump = openFileDialog("Select Korg MS2000 bulk dump file", SUPPORTED_EXT_MASK)
	local dumpBytes = MemoryBlock()

	-- If file exists, then proceed
	if bulkDump ~= nil then
		dumpBytes = MemoryBlock(bulkDump:getSize())
		bulkDump:loadFileAsData(dumpBytes)
	else
		return
	end

	local rawDumpBytesData = normalizeSysExDumpData(memBlockToTable(dumpBytes))
	local dumpType = checkBulkDumpSize(#rawDumpBytesData)

	if dumpType ~= dtInvalidSz then

		if dumpType == dtHandson then

			-- Check microKORG *.prg signature
			if (rawDumpBytesData[3] == 0xA4) and (rawDumpBytesData[4] == 0x0F) then

				rawDumpBytesData = cutBytesFromDump(rawDumpBytesData, 2, 3)
			end
		end

		presetBank = slicePresets(midiToProgramData(rawDumpBytesData, DATA_PREAMBLE_BYTES))

		dataBuffer = copyTable(presetBank[1][1])

		chosenPresetToBuffer(101, false) -- 1 - bank, 01 - preset number
		-- Congratulations! MS2000 bulk dump successfully imported!
	else

		genAlertWindow("Warning", "Wrong file size, operation cancelled")
	end
end
```

---

## `saveProgramBankFile`

```lua
function saveProgramBankFile()
	
	-- Default filename will be like current preset name
	local cPresName = "ReMS2000 Bulk Dump " .. os.date("%Y %m %d")

	-- File to save
	local dumpFile = utils.saveFileWindow("Save program bank to file..", File(removeSystemSymbols(cPresName)), "*.syx", true)

	if not dumpFile:isValid() then
		return
	end

	-- Writing data to the file
	dumpFile:replaceWithData(MemoryBlock(prepareBulkDump()))
end
```

---

## `saveProgramToPatchBank`

```lua
function saveProgramToPatchBank(rawNumber)
	
	local bank = math.floor((rawNumber - 1000) / 100)
	local preset = rawNumber % 100
	local patchData

	syncPanelWithBuffer()

	-- Buffer ==> presetBank[bank][preset]

	-- If timbre type is vocoder, we have to merge data first
	if sharedValues.timbreMode == tmSynth then
		presetBank[bank][preset] = copyTable(dataBuffer)
	else
		presetBank[bank][preset] = getMergedTimbreVocoderData()
	end

	-- Run assertions by opening saved program from bank
	chosenPresetToBuffer(rawNumber, true)
end
```

---

## `storeProgramBank`

```lua
function storeProgramBank()
	
	sendSysExMessage(prepareBulkDump())
	waitForWriteReply(WAIT_BANK_TIMER)
end
```

---

## `prepareBulkDump`

```lua
function prepareBulkDump()
	
	local i, j, c
	local midiCh = getGlobalMidiChannel()
	local bulkDump = {0xF0, 0x42, midiCh, 0x58, 0x4C}

	-- Cut extra bytes (SysEx Header, SysEx End) from every program
	for i = 1, 8 do
		for j = 1, 16 do

			for c = (DATA_PREAMBLE_BYTES + 1), #presetBank[i][j] - 1 do
				table.insert(bulkDump, presetBank[i][j][c])
			end
		end
	end

	table.insert(bulkDump, 0xF7)

	return programToMIDIData(bulkDump, DATA_PREAMBLE_BYTES)
end
```

---

## `getMergedTimbreVocoderData`

```lua
function getMergedTimbreVocoderData()
	
	-- Merge dataBuffer with vocoder data

	local i
	local mergedData = copyTable(dataBuffer)

	for i = TIMBRE_ONE_STARTBYTE, TIMBRE_ONE_STARTBYTE + (TIMBRE_DATA_SIZE * 2) - 1 do

		mergedData[i] = vocoderBuffer[i - TIMBRE_ONE_STARTBYTE + 1]
	end

	return mergedData
end
```

---

## `checkBulkDumpSize`

```lua
function checkBulkDumpSize(dumpSize)
	
	-- Check if opened data size is correct
	-- Return type of dump, if size is correct

	local result = dtInvalidSz

	if dumpSize == PROGRAM_BANK_DUMP_SIZE then
		result = dtProgBank
	elseif dumpSize == ALL_DATA_DUMP_SIZE then
		result = dtAllData
	elseif dumpSize == HANDSON_DUMP_SIZE then
		result = dtHandson
	end

	return result
end
```

---

## `cutBytesFromDump`

```lua
function cutBytesFromDump(sourceTable, bFrom, bCount)

	local i
	local result = {}

	for i = 1, #sourceTable do

		if (i < bFrom) or (i >= (bFrom + bCount)) then

			table.insert(result, sourceTable[i])
		end
	end

	return result
end
```

---

## `blinkMidiLightTimer`

```lua
function blinkMidiLightTimer()

	timer:setCallback (BLINKMIDI_TIMER_ID, blinkMidiLightTimerCallback)
	timer:stopTimer(BLINKMIDI_TIMER_ID) 
	timer:startTimer(BLINKMIDI_TIMER_ID, BLINKMIDI_TIMER)
end

function blinkMidiLightTimerCallback()

 	sharedValues.midiActivity = 0
	externalRepaintMidiActivity()

	timer:stopTimer(BLINKMIDI_TIMER_ID)
end
```

---

## `requestProgramBank`

```lua
function requestProgramBank()
	
	-- Request program bank
	sendSysExMessage({0xF0, 0x42, getGlobalMidiChannel(), 0x58, 0x1C, 0xF7})

	-- Stop timer if it's already running
	timer:stopTimer(WAIT_BANK_TIMER_ID)

	timerFlags.waitForBulkDump	= true
	setSynthReachStatus(dsBusy)

	timer:setCallback(WAIT_BANK_TIMER_ID, requestProgramBankCallback)
	timer:startTimer(WAIT_BANK_TIMER_ID, WAIT_BANK_TIMER)
end

function requestProgramBankCallback()

 	-- Set synth error on timeout
	if timerFlags.waitForBulkDump == true then
		setSynthReachStatus(dsError, true)
	end

	timerFlags.waitForBulkDump	= false

	timer:stopTimer(WAIT_BANK_TIMER_ID)
end
```

---

## `captureProgramDumpData`

```lua
function captureProgramDumpData(programData)

	timerFlags.waitForSingleProgram	= false
	setSynthReachStatus(dsOnline, true)

	if programData:getSize() == SINGLE_PROGRAM_SIZE then

		local rawDumpBytesData = normalizeSysExDumpData(memBlockToTable(programData))

		dataBuffer = midiToProgramData(rawDumpBytesData, DATA_PREAMBLE_BYTES)

		applyProgramData(dataBuffer, nil, nil, true)

		showHint("Info: Program dump received")
	else
		setSynthReachStatus(dsError)
		showHint("Info: Error during program data transmission")
	end
end
```

---

## `captureBulkDumpData`

```lua
function captureBulkDumpData(programData)

	timerFlags.waitForBulkDump = false
	setSynthReachStatus(dsOnline, true)

	if (programData:getSize() == PROGRAM_BANK_DUMP_SIZE) or (programData:getSize() == ALL_DATA_DUMP_SIZE) then

		local rawDumpBytesData = normalizeSysExDumpData(memBlockToTable(programData))

		presetBank = slicePresets(midiToProgramData(rawDumpBytesData, DATA_PREAMBLE_BYTES))
		dataBuffer = copyTable(presetBank[1][1])
		chosenPresetToBuffer(101, true)

		showHint("Info: SysEx bulk dump received")
	else

		setSynthReachStatus(dsError)
		showHint("Info: Error during program data transmission")
	end
end
```

---

