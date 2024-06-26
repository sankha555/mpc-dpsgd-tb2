#pragma once
#include <libOTe/config.h>
#ifdef ENABLE_SOFTSPOKEN_OT

#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Network/Channel.h>
#include "DotSemiHonest.h"

namespace osuCrypto
{
namespace SoftSpokenOT
{

// Hash DotSemiHonest to get a random OT.

class TwoOneSemiHonestSender :
	public DotSemiHonestSender,
	private ChunkedReceiver<
		TwoOneSemiHonestSender,
		std::tuple<std::array<block, 2>>,
		std::tuple<AlignedBlockPtrT<std::array<block, 2>>>
	>
{
public:
	using Base = DotSemiHonestSender;

	TwoOneSemiHonestSender(size_t fieldBits, size_t numThreads_ = 1) :
		Base(fieldBits, numThreads_),
		ChunkerBase(this) {}

	TwoOneSemiHonestSender splitBase()
	{
		throw RTE_LOC; // TODO: unimplemented.
	}

	std::unique_ptr<OtExtSender> split() override
	{
		return std::make_unique<TwoOneSemiHonestSender>(splitBase());
	}

	virtual void initTemporaryStorage() override { ChunkerBase::initTemporaryStorage(); }

	void send(span<std::array<block, 2>> messages, PRNG& prng, Channel& chl) override;

	// Low level functions.

	// Perform up to 128 random OTs (assuming that the messages have been received from the
	// receiver), and output the message pairs. Set numUsed to be < 128 if you don't neeed all of
	// the messages. The number of blocks in messages (2 * messages.size()) must be at least
	// wPadded(), as there might be some padding. Also, messages must be given the alignment of an
	// AlignedBlockArray.
	void generateRandom(size_t blockIdx, const AES& aes, size_t numUsed,
	                    span<std::array<block, 2>> messages)
	{
		block* messagesPtr = (block*) messages.data();
		Base::generateRandom(blockIdx, aes, span<block>(messagesPtr, wPadded()));
		xorAndHashMessages(numUsed, delta(), messagesPtr, messagesPtr, aes);
	}

	void generateChosen(size_t blockIdx, const AES& aes, size_t numUsed,
	                    span<std::array<block, 2>> messages)
	{
		block* messagesPtr = (block*) messages.data();
		Base::generateChosen(blockIdx, aes, span<block>(messagesPtr, wPadded()));
		xorAndHashMessages(numUsed, delta(), messagesPtr, messagesPtr, aes);
	}

	// messagesOut and messagesIn must either be equal or non-overlapping.
	template<typename Enc>
	static void xorAndHashMessages(
		size_t numUsed, block deltaBlock, block* messagesOut, const block* messagesIn, Enc& enc)
	{
		// Loop backwards, similarly to DotSemiHonest.
		size_t i = numUsed;
		while (i >= superBlkSize / 2)
		{
			i -= superBlkSize / 2;

			// Temporary array, so I (and the compiler) don't have to worry so much about aliasing.
			block superBlk[superBlkSize];
			for (size_t j = 0; j < superBlkSize / 2; ++j)
			{
				superBlk[2*j] = messagesIn[i + j];
				superBlk[2*j + 1] = messagesIn[i + j] ^ deltaBlock;
			}

			enc.template hashBlocks<superBlkSize>(superBlk, messagesOut + 2*i);
		}

		// Finish up. The more straightforward while (i--) unfortunately gives a (spurious AFAICT)
		// compiler warning about undefined behavior at iteration 0xfffffffffffffff, so use a for loop.
		size_t remainingIters = i;
		for (size_t j = 0; j < remainingIters; ++j)
		{
			i = remainingIters - j - 1;

			block msgs[2];
			msgs[0] = messagesIn[i];
			msgs[1] = msgs[0] ^ deltaBlock;
			enc.template hashBlocks<2>(msgs, messagesOut + 2*i);
		}

		// Note: probably need a stronger hash for malicious secure version.
	}

	TRY_FORCEINLINE void processChunk(
		size_t nChunk, size_t numUsed, span<std::array<block, 2>> messages);

protected:
	using ChunkerBase = ChunkedReceiver<
		TwoOneSemiHonestSender,
		std::tuple<std::array<block, 2>>,
		std::tuple<AlignedBlockPtrT<std::array<block, 2>>>
	>;
	friend ChunkerBase;
	friend ChunkerBase::Base;
};

class TwoOneSemiHonestReceiver :
	public DotSemiHonestReceiver,
	private ChunkedSender<TwoOneSemiHonestReceiver, std::tuple<block>, std::tuple<AlignedBlockPtr>>
{
public:
	using Base = DotSemiHonestReceiver;

	TwoOneSemiHonestReceiver(size_t fieldBits, size_t numThreads_ = 1) :
		Base(fieldBits, numThreads_),
		ChunkerBase(this) {}

	TwoOneSemiHonestReceiver splitBase()
	{
		throw RTE_LOC; // TODO: unimplemented.
	}

	std::unique_ptr<OtExtReceiver> split() override
	{
		return std::make_unique<TwoOneSemiHonestReceiver>(splitBase());
	}

	virtual void initTemporaryStorage() override { ChunkerBase::initTemporaryStorage(); }

	void receive(const BitVector& choices, span<block> messages, PRNG& prng, Channel& chl) override;

	// Low level functions.

	// Perform 128 random OTs (saving the messages up to send to the sender), and output the choice
	// bits (packed into a 128 bit block) and the chosen messages. Set numUsed to be < 128 if you
	// don't neeed all of the messages. messages.size() must be at least vPadded(), as there might
	// be some padding. Also, messages must be given the alignment of an AlignedBlockArray.
	void generateRandom(size_t blockIdx, const AES& aes, size_t numUsed,
	                    block& choicesOut, span<block> messages)
	{
		Base::generateRandom(blockIdx, aes, choicesOut, messages);
		aes.hashBlocks(messages.data(), numUsed, messages.data());
	}

	void generateChosen(size_t blockIdx, const AES& aes, size_t numUsed,
	                    block choicesIn, span<block> messages)
	{
		Base::generateChosen(blockIdx, aes, choicesIn, messages);
		aes.hashBlocks(messages.data(), numUsed, messages.data());
	}

	TRY_FORCEINLINE void processChunk(
		size_t nChunk, size_t numUsed, span<block> messages, block chioces);

protected:
	using ChunkerBase = ChunkedSender<
		TwoOneSemiHonestReceiver,
		std::tuple<block>,
		std::tuple<AlignedBlockPtr>
	>;
	friend ChunkerBase;
	friend ChunkerBase::Base;
};

}
}
#endif
