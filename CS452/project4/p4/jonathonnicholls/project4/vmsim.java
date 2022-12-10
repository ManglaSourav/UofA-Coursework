package project4CSC452;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.Scanner;
/*
 * I created the algorithms of the clock and NFU but I have not tested them.
 * To my knowledge, I am able to find page number to addresses.
 */
public class vmsim {

	private static int numFrames;
	private static String alg;
	private static String fileName;
	private static int totalFaults;
	private static int totalMem;
	private static int totalWrites;
	private static int sizeOfTable;
	private static File file;
	private static ArrayList<PageTableEntry> list;
	private static ArrayList<PageTableEntry> workingSet;
	final static int pageSize = 16384; //8KB
	private static PageTableEntry head;
	final static int totalPages = 262144;
	private static ArrayList<Integer> emptyFrames;
	
	public static void main(String args[]) throws FileNotFoundException {
		if(args.length != 5) {
			System.out.println("Incorrect number of arguments");
			System.exit(1);
		}
		
		for(int i = 0; i < args.length; i++) {
			if(args[i].equals("-n")) {
				numFrames = Integer.parseInt(args[i+1]);
				i++;
			}
			if(args[i].equals("-a")) {
				alg = args[i+1];
				i++;
			}
		}
		fileName = args[args.length-1];
		file = new File(fileName);
		
		createdFrames();
		startSim();		
		printOutput();
	}
	
	private static void startSim() throws FileNotFoundException {
		Scanner reader = new Scanner(file); 
		while(reader.hasNextLine()) {
			String line = reader.nextLine();
			String[] temp = readInput(line);
			boolean flag = runInst(temp);
			if(flag) {
				flag = runInst(temp);
			}
		}
		reader.close();
	}

	/*
	 * Running instructions from the input file
	 */
	private static boolean runInst(String[] temp) {
		boolean flag;
		int pageNum = MMUAlg(temp[1]);
		PageTableEntry e = list.get(pageNum);
		if(e.isValid()) { //is valid and has a frame
			System.out.println("For "+ temp[1]+", its page is "+pageNum+" and its frame is "+e.getFrameId()+"\n");
			flag = false;
			eventHandle(temp, e);
		}else { //page fault has occurred
			flag = true;
			totalFaults++;
			if(!emptyFrames.isEmpty()) {
				int frameNum = emptyFrames.get(0);
				emptyFrames.remove(0);
				e.setFrameId(frameNum);
				workingSet.add(e);
			}else {
				PageTableEntry castOut = eviction(temp);
				if(castOut.isDirty()) {
					totalWrites++;
					castOut.setDirty(false);
					int frame = castOut.getFrameId();
					castOut.freeFrame();
					castOut.setValid(false);
					e.setFrameId(frame);
					workingSet.remove(castOut);
					workingSet.add(e);
				}
			}
		}
		return flag;
	}
	
	/*
	 * Frees a frame from a PageTableEntry depending on the algorithm
	 */
	private static PageTableEntry eviction(String[] temp) {
		PageTableEntry out = null;
		if(alg.equals("opt")) {
			out = optAlg();
		}else if(alg.equals("clock")) {
			out = clockAlg();
		}else if(alg.equals("lru")) {
			out = lruAlg();
		}else if(alg.equals("nfu")) {
			out = nfuAlg();
		}
		return out;
	}

	/*
	 * Handles what happens depending on the instruction type
	 */
	private static void eventHandle(String[] temp, PageTableEntry e) {
		if(temp[0].equals("I")) {
			totalMem++;
			e.setReferenced(true);
		}else if(temp[0].equals("S")) {
			totalWrites++;
			e.setDirty(false);
			e.setReferenced(true);
		}else if(temp[0].equals("L")) {
			e.setDirty(true);
			e.setReferenced(true);
		}else if(temp[0].equals("M")) {
			totalWrites++;
			e.setDirty(false);
			e.setReferenced(true);
		}
	}

	/*
	 * Created enough PageTableEntries for all possible pages
	 */
	private static void createdFrames() {
		list = new ArrayList<>();
		for(int i = 0; i < totalPages; i++) {
			PageTableEntry temp = new PageTableEntry(i);
			list.add(temp);
		}
		emptyFrames = new ArrayList<>();
		for(int i = 0; i < numFrames; i++) {
			emptyFrames.add(i);
		}
	}
	
	/*
	 * Parses the line to instruction type and address
	 */
	private static String[] readInput(String line) {
		String[] output = new String[2];
		String[] temp = line.split("");
		output[0] = temp[0];
		output[1] = temp[0].split(",")[0];
		return output;
	}
	
	/*
	 * Implementation of clock algorithm
	 */
	private static PageTableEntry clockAlg() {
		if (head.equals(null)) {
			head = workingSet.get(0);
		}
		int i = workingSet.indexOf(head);
		while(head.isReferenced()) {
			i++;
			if(i > workingSet.size()) {
				i = 0;
			}
			head.setReferenced(false);
			head = workingSet.get(i);
		}
		PageTableEntry out = head;
		head = workingSet.get(i);
		return out;
	}
	private static PageTableEntry optAlg() {
		return null;
	}
	private static PageTableEntry lruAlg() {
		return null;
	}
	/*
	 * Implementation of NFU algorithm
	 */
	private static PageTableEntry nfuAlg() {
		PageTableEntry out = null;
		for(PageTableEntry e : workingSet) {
			if(out == null) {
				out = e;
			}
			if(e.getRCount() < out.getRCount()) {
				out = e;
			}
		}
		for(PageTableEntry e : workingSet) {
			e.resetRCount();
		}
		return out;
	}
	
	/*
	 * Finds the page number from the VM address
	 */
	private static int MMUAlg(String vmAddress) {
		return Integer.parseInt(vmAddress, 16) / pageSize;	
	}

	/*
	 * Prints out the info from the end
	 */
	private static void printOutput() {
		System.out.print("Algorithm: "+alg+"\nNumber of frames:\t"+numFrames+"\nTotal memory accesses:\t"+totalMem+"\nTotal page faults:\t"+totalFaults+"\n"
				+ "Total writes to disk:\t"+totalWrites+"\nTotal size of page table:\t"+sizeOfTable+" bytes\n");
	}
	
}
