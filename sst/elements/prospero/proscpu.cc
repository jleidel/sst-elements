
#include "sst_config.h"
#include "proscpu.h"
#include <algorithm>

using namespace SST;
using namespace SST::Prospero;

#define PROSPERO_MAX(a, b) ((a) < (b) ? (b) : (a))

ProsperoComponent::ProsperoComponent(ComponentId_t id, Params& params) :
	Component(id) {

	const uint32_t output_level = (uint32_t) params.find_integer("verbose", 0);
	output = new SST::Output("Prospero[@p:@l]: ", output_level, 0, SST::Output::STDOUT);

	std::string traceModule = params.find_string("reader", "prospero.ProsperoTextTraceReader");
	output->verbose(CALL_INFO, 1, 0, "Reader module is: %s\n", traceModule.c_str());

	Params readerParams = params.find_prefix_params("readerParams.");
	reader = dynamic_cast<ProsperoTraceReader*>( loadModuleWithComponent(traceModule, this, readerParams) );

	if(NULL == reader) {
		output->fatal(CALL_INFO, -1, "Failed to load reader module: %s\n", traceModule.c_str());
	}

	reader->setOutput(output);

	pageSize = (uint64_t) params.find_integer("pagesize", 4096);
	output->verbose(CALL_INFO, 1, 0, "Configured Prospero page size for %" PRIu64 " bytes.\n", pageSize);

        cacheLineSize = (uint64_t) params.find_integer("cache_line_size", 64);
	output->verbose(CALL_INFO, 1, 0, "Configured Prospero cache line size for %" PRIu64 " bytes.\n", cacheLineSize);

	std::string prosClock = params.find_string("clock", "2GHz");
	// Register the clock
	registerClock(prosClock, new Clock::Handler<ProsperoComponent>(this, &ProsperoComponent::tick));

	output->verbose(CALL_INFO, 1, 0, "Configured Prospero clock for %s\n", prosClock.c_str());

	maxOutstanding = (uint32_t) params.find_integer("max_outstanding", 16);
	output->verbose(CALL_INFO, 1, 0, "Configured maximum outstanding transactions for %" PRIu32 "\n", maxOutstanding);

	maxIssuePerCycle = (uint32_t) params.find_integer("max_issue_per_cycle", 2);
	output->verbose(CALL_INFO, 1, 0, "Configured maximum transaction issue per cycle %" PRIu32 "\n", maxIssuePerCycle);

	// tell the simulator not to end without us
  	registerAsPrimaryComponent();
  	primaryComponentDoNotEndSim();

	output->verbose(CALL_INFO, 1, 0, "Configuring Prospero cache connection...\n");
	cache_link = dynamic_cast<SimpleMem*>(loadModuleWithComponent("memHierarchy.memInterface", this, params));
  	cache_link->initialize("cache_link", new SimpleMem::Handler<ProsperoComponent>(this,
		&ProsperoComponent::handleResponse) );
	output->verbose(CALL_INFO, 1, 0, "Configuration of memory interface completed.\n");

	output->verbose(CALL_INFO, 1, 0, "Reading first entry from the trace reader...\n");
	currentEntry = reader->readNextEntry();
	output->verbose(CALL_INFO, 1, 0, "Read of first entry complete.\n");

	output->verbose(CALL_INFO, 1, 0, "Creating memory manager with page size %" PRIu64 "...\n", pageSize);
	memMgr = new ProsperoMemoryManager(pageSize, output);
	output->verbose(CALL_INFO, 1, 0, "Created memory manager successfully.\n");

	// We start by telling the system to continue to process as long as the first entry
	// is not NULL
	traceEnded = currentEntry == NULL;

	readsIssued = 0;
	writesIssued = 0;
	splitReadsIssued = 0;
	splitWritesIssued = 0;
	totalBytesRead = 0;
	totalBytesWritten = 0;

	currentOutstanding = 0;

	output->verbose(CALL_INFO, 1, 0, "Prospero configuration completed successfully.\n");
}

ProsperoComponent::~ProsperoComponent() {
	delete memMgr;
	delete output;
}

void ProsperoComponent::finish() {
	output->output("Prospero Component Statistics:\n");
	output->output("- Reads issued:             %" PRIu64 "\n", readsIssued);
	output->output("- Writes issued:            %" PRIu64 "\n", writesIssued);
	output->output("- Split reads issued:       %" PRIu64 "\n", splitReadsIssued);
	output->output("- Split writes issued:      %" PRIu64 "\n", splitWritesIssued);
	output->output("- Bytes read:               %" PRIu64 "\n", totalBytesRead);
	output->output("- Bytes written:            %" PRIu64 "\n", totalBytesWritten);

	const uint64_t nanoSeconds = getCurrentSimTimeNano();
	output->output("- Bandwidth (read):         %" PRIu64 "B/s\n",
		(PROSPERO_MAX(totalBytesRead, 1) / PROSPERO_MAX(nanoSeconds / 1000000000, 1)));
	output->output("- Bandwidth (written):      %" PRIu64 "B/s\n",
		(PROSPERO_MAX(totalBytesWritten, 1) / PROSPERO_MAX(nanoSeconds / 1000000000, 1)));
	output->output("- Bandwidth (combined):     %" PRIu64 "B/s\n",
		(PROSPERO_MAX(totalBytesWritten + totalBytesRead, 1)
		/ PROSPERO_MAX(nanoSeconds / 1000000000, 1)));
}

void ProsperoComponent::handleResponse(SimpleMem::Request *ev) {
	output->verbose(CALL_INFO, 4, 0, "Handle response from memory subsystem.\n");

	currentOutstanding--;

	// Our responsibility to delete incoming event
	delete ev;
}

bool ProsperoComponent::tick(SST::Cycle_t currentCycle) {
	if(NULL == currentEntry) {
		output->verbose(CALL_INFO, 16, 0, "Prospero execute on cycle %" PRIu64 ", current entry is NULL, outstanding=%" PRIu32 ", maxOut=%" PRIu32 "\n",
			(uint64_t) currentCycle, currentOutstanding, maxOutstanding);
	} else {
		output->verbose(CALL_INFO, 16, 0, "Prospero execute on cycle %" PRIu64 ", current entry time: %" PRIu64 ", outstanding=%" PRIu32 ", maxOut=%" PRIu32 "\n",
			(uint64_t) currentCycle, (uint64_t) currentEntry->getIssueAtCycle(),
			currentOutstanding, maxOutstanding);
	}

	// If we have finished reading the trace we need to let the events in flight
	// drain and the system come to a rest
	if(traceEnded) {
		if(0 == currentOutstanding) {
			primaryComponentOKToEndSim();
                        return true;
		}

		return false;
	}

	// Wait to see if the current operation can be issued, if yes then
	// go ahead and issue it, otherwise we will stall
	for(uint32_t i = 0; i < maxIssuePerCycle; ++i) {
		if(currentCycle >= currentEntry->getIssueAtCycle()) {
			if(currentOutstanding < maxOutstanding) {
				// Issue the pending request into the memory subsystem
				issueRequest(currentEntry);

				// Obtain the next newest request
				currentEntry = reader->readNextEntry();

				// Trace reader has read all entries, time to begin draining
				// the system, caches etc
				if(NULL == currentEntry) {
					traceEnded = true;
					break;
				}
			} else {
				// Cannot issue any more items this cycle, load/stores are full
				break;
			}
		} else {
			output->verbose(CALL_INFO, 8, 0, "Not issuing on cycle %" PRIu64 ", waiting for cycle: %" PRIu64 "\n",
				(uint64_t) currentCycle, currentEntry->getIssueAtCycle());
			// Have reached a point in the trace which is too far ahead in time
			// so stall until we find that point
			break;
		}
	}

	// Keep simulation ticking, we have more work to do if we reach here
	return false;
}

void ProsperoComponent::issueRequest(const ProsperoTraceEntry* entry) {
	const uint64_t entryAddress = entry->getAddress();
	const uint64_t entryLength  = (uint64_t) entry->getLength();

	const uint64_t lineOffset   = entryAddress % cacheLineSize;
	bool  isRead                = entry->isRead();

	if(isRead) {
		totalBytesRead += entryLength;
	} else {
		totalBytesWritten += entryLength;
	}

	if(lineOffset + entryLength > cacheLineSize) {
		// Perform a split cache line load
		const uint64_t lowerLength = cacheLineSize - lineOffset;
		const uint64_t upperLength = entryLength - lowerLength;

		if(lowerLength + upperLength != entryLength) {
			output->fatal(CALL_INFO, -1, "Error: split cache line, lower size=%" PRIu64 ", upper size=%" PRIu64 " != request length: %" PRIu64 " (cache line %" PRIu64 ")\n",
				lowerLength, upperLength, entryLength, cacheLineSize);
		}
		assert(lowerLength + upperLength == entryLength);

		// Start split requests at the original requested address and then
		// also the the next cache line along
		const uint64_t lowerAddress = memMgr->translate(entryAddress);
		const uint64_t upperAddress = memMgr->translate((lowerAddress - (lowerAddress % cacheLineSize)) + cacheLineSize);

		SimpleMem::Request* reqLower = new SimpleMem::Request(
			isRead ? SimpleMem::Request::Read : SimpleMem::Request::Write,
			lowerAddress, lowerLength);

		SimpleMem::Request* reqUpper = new SimpleMem::Request(
			isRead ? SimpleMem::Request::Read : SimpleMem::Request::Write,
			upperAddress, upperLength);

		cache_link->sendRequest(reqLower);
		cache_link->sendRequest(reqUpper);

		if(isRead) {
			readsIssued += 2;
			splitReadsIssued++;
		} else {
			writesIssued += 2;
			splitWritesIssued++;
		}

		currentOutstanding++;
		currentOutstanding++;
	} else {
		// Perform a single load
		SimpleMem::Request* request = new SimpleMem::Request(
			isRead ? SimpleMem::Request::Read : SimpleMem::Request::Write,
			memMgr->translate(entryAddress), entryLength);
		cache_link->sendRequest(request);

		if(isRead) {
			readsIssued++;
		} else {
			writesIssued++;
		}

		currentOutstanding++;
	}

	// Delete this entry, we are done converting it into a request
	delete entry;
}
