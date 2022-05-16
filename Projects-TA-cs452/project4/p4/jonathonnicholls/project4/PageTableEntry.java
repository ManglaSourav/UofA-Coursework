package project4CSC452;

public class PageTableEntry {

	private int id; //page number
	private boolean dirty;
	private boolean referenced;
	private boolean valid;
	private int frameId; //frame number
	private int referencedCount; //used for the NFU algorithm
	
	public PageTableEntry(int id) {
		this.id = id;
		referenced = false;
		dirty = false;
		valid = false;
		frameId = -1;
	}

	public boolean isReferenced() {
		return referenced;
	}

	public boolean isDirty() {
		return dirty;
	}

	public int getId() {
		return id;
	}

	public boolean isValid() {
		return valid;
	}
	
	public void setDirty(boolean dirty) {
		this.dirty = dirty;
	}

	public void setReferenced(boolean referenced) {
		this.referenced = referenced;
		referencedCount++;
	}

	public void setValid(boolean valid) {
		this.valid = valid;
	}
	
	public void setFrameId(int id) {
		frameId = id;
		referencedCount = 1;
	}
	
	public int getFrameId() {
		return frameId;
	}
	
	public void freeFrame() {
		frameId = -1;
	}
	
	public int getRCount() {
		return referencedCount;
	}
	
	public void resetRCount() {
		referencedCount = 0;
	}

}