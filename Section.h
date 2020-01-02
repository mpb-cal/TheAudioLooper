#ifndef __SECTION_H__
#define __SECTION_H__

#define MOVE_LEFT		0
#define MOVE_RIGHT	1

class Section
{
	public:

	Section();

	void setSecondsPerBeat( double );
	void setBeatsPerMinute( double );

	void setStart( __int64 );
	void setStop( __int64 );
	void setLength( __int64 );
	void setDuration( __int64 );

	void setStartInSeconds( double );
	void setStopInSeconds( double );
	void setLengthInSeconds( double );

	void setStartInBeats( double );
	void setStopInBeats( double );
	void setLengthInBeats( double );

	void moveStartInSeconds( int dir, double seconds );
	void moveStopInSeconds( int dir, double seconds );

	double getSecondsPerBeat();
	double getBeatsPerMinute();

	__int64 getStart();
	__int64 getStop();
	__int64 getLength();
	__int64 getDuration();

	double getStartInSeconds();
	double getStopInSeconds();
	double getLengthInSeconds();

	double getStartInBeats();
	double getStopInBeats();
	double getLengthInBeats();

	private:

	double		m_secondsPerBeat;
	double		m_beatsPerMinute;

	__int64		m_startPos;
	__int64		m_stopPos;
	__int64		m_length;
	__int64		m_duration;
};



#endif

