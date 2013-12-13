#include <Vector.h>

namespace GSM {
	class SCHL1Decoder {
	private:
		Parity mParity;
		BitVector mU;
		BitVector mD;
		SoftVector mE;
		SoftVector mE1;
		SoftVector mE2;

		ViterbiR2O4 mVCoder;

		int mNCC;
		int mBCC;
		int mT1;
		int mT2;
		int mT3p;

	public:
		SCHL1Decoder();
		int writeLowSide(SoftVector&);
		void reset();

		int getNCC() { return mNCC; }
		int getBCC() { return mBCC; }
		int getT1() { return mT1; }
		int getT2() { return mT2; }
		int getT3p() { return mT3p; }
	};
};
