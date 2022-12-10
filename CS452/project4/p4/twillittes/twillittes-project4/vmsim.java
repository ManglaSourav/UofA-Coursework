import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Random;

/*
 * Taylor Willittes
 * 452 Proj 4
 * File: vmsim.java
 * Puropse: compare/contrast 4 diff page replacement algs w the num of PF and WTD.
 * opt - page replacement alg that has perf knowledge of futyre
 * clock - improved version of second chance alg
 * lru - evicts the least recently used
 * nfu - evicts the page that has the lowest refrence count
 * Usage: vmsim –n <numframes> -a <opt|clock|lru|nfu> <tracefile>
 * Usage on lectura: java vmsim –n <numframes> -a <opt|clock|lru|nfu> <tracefile>
 */
public class vmsim {
	
	//globs
	static int numOfPageFaults = 0;
	static int accesses = 0;
	static int writesToDisk = 0;
	static int sizep = (int) ((Math.pow(2, 32)/Math.pow(2, 13)) * Math.pow(2, 13));
	
	public static void main(String[] args) throws IOException {
		
		//for eclipse
		//int numframes = Integer.valueOf(args[2]);
		//String alg = args[4];
		//String tracefile = args[5];
		
		//for lectura
		int numframes = Integer.valueOf(args[1]);
		String alg = args[3];
		String tracefile = args[4];
		
		FileReader input = new FileReader(tracefile);
		BufferedReader bf = new BufferedReader(input);
		String temp = bf.readLine();
		
		pagetable pt = new pagetable(numframes);
		if (alg.equals("opt")) {
			preproc(tracefile, pt); //calls preproc func
		}
		//int count2 = 0;
		while (temp != null)
		{
			//System.out.println("T" + temp);
			/*
			if (pt.map.size() == 8 && count2 > 10) {
				//count2 ++;
				break;
			}
			if (pt.map.size() == 8) {
				count2 ++;
			}*/
			int temp2 = 0;
			if (temp.charAt(0) == 'I') //if there is an I
			{
				String[] t = temp.split(" ");
				//System.out.println("T1 " + Arrays.toString(t));
				String[] newt = t[2].split(",");
				//System.out.println("NT " + Arrays.toString(newt));
				temp2 = pt.accessMemory("I", Long.parseLong(newt[0], 16));
				accesses ++;
				
			} else if (temp.charAt(1) == 'L') { //if there is an L
				String[] t = temp.split(" ");
				String[] newt = t[2].split(",");
				temp2 = pt.accessMemory("L", Long.parseLong(newt[0], 16));
				accesses ++;
				
			} else if (temp.charAt(1) == 'S') { //if there is an S
				String[] t = temp.split(" ");
				//System.out.println("T1 " + Arrays.toString(t));
				String[] newt = t[2].strip().split(",");
				//System.out.println("NT " + Arrays.toString(newt));
				temp2 = pt.accessMemory("S", Long.parseLong(newt[0], 16));
				accesses ++;
				
			} else if (temp.charAt(1) == 'M') { //if there is an M
				String[] t = temp.split(" ");
				String[] newt = t[2].split(",");
				temp2 = pt.accessMemory("M", Long.parseLong(newt[0], 16));
				accesses ++;
			} else {
				temp = bf.readLine();
				continue;
			}
			//calls correct alg
			if (alg.equals("opt")) {
				//System.out.println("opt ");
				//preproc(tracefile, pt, accesses);
				opt(temp2, pt, numframes, tracefile, accesses);
			} else if (alg.equals("clock")) {
				//System.out.println("clock ");
				clock(temp2, pt, numframes);
			} else if (alg.equals("lru")) {
				//System.out.println("lru ");
				lru(temp2, pt, numframes);
			} else if (alg.equals("nru")) {
				//System.out.println("nfu ");
				//System.out.println("HERE");
				nru(temp2, pt, numframes);
			} else if (alg.equals("nfu")) {
				nfu(temp2, pt, numframes);
			} else {
				System.out.println("Wrong"); //wrong alg given
			}
			temp = bf.readLine(); //read next line
		}
		
		//prints output of the algs when they complete
		System.out.println("Algorithm: " + alg);
		System.out.println("Number of frames: " + numframes);
		System.out.println("Total memory accesses: " + accesses);
		System.out.println("Total page faults: " + numOfPageFaults);
		System.out.println("Total writes to disk: " + writesToDisk);
		System.out.println("Total size of page table: " + sizep + " bytes");

	}
	
	/*
	 * This function is the preprocessor for the opt function. Reads file and modifies pagetable
	 * accordingly.
	 * 
	 * tracefile - String of the tracefile
	 * pt - pagetable obj that reps a page table
	 */
	public static int preproc(String tracefile, pagetable pt) throws IOException {
		FileReader input = new FileReader(tracefile);
		BufferedReader bf = new BufferedReader(input);
		String temp = bf.readLine();
		int tempint = -1;
		//pagetable pt = new pagetable(numframes);
		int count = 0;
		while (temp != null)
		{
			//if (count < start) {
			//	count = 0;
			//}
			//int pagenum = (int) (Long.parseLong(newt[0], 16)/Math.pow(2, 11));
			int temp2 = 0;
			//int count = 0;
			if (temp.charAt(0) == 'I')
			{
				String[] t = temp.split(" ");
				//System.out.println("T1 " + Arrays.toString(t));
				String[] newt = t[2].split(",");
				//System.out.println("NT " + Arrays.toString(newt));
				temp2 = (int) (Long.parseLong(newt[0], 16)/Math.pow(2, 13));
				pt.pages.get(temp2).ins.add(count); //adds count to ins field
				//System.out.println("temp1 " + temp2);
				//System.out.println("in1 " + pt.pages.get(temp2).ins);
				count ++;
				
				//accesses ++;
				
			} else if (temp.charAt(1) == 'L') {
				String[] t = temp.split(" ");
				String[] newt = t[2].split(",");
				temp2 = (int) (Long.parseLong(newt[0], 16)/Math.pow(2, 13));
				pt.pages.get(temp2).ins.add(count);
				//System.out.println("temp2 " + temp2);
				//System.out.println("in2 " + pt.pages.get(temp2).ins);
				count ++;
				//accesses ++;
				
			} else if (temp.charAt(1) == 'S') {
				String[] t = temp.split(" ");
				//System.out.println("T1 " + Arrays.toString(t));
				String[] newt = t[2].strip().split(",");
				//System.out.println("NT " + Arrays.toString(newt));
				temp2 = (int) (Long.parseLong(newt[0], 16)/Math.pow(2, 13));
				pt.pages.get(temp2).ins.add(count); //adds count to ins field
				//System.out.println("temp3 " + temp2 );
				//System.out.println("in3 " + pt.pages.get(temp2).ins);
				count ++;
				//accesses ++;
				
			} else if (temp.charAt(1) == 'M') {
				String[] t = temp.split(" ");
				String[] newt = t[2].split(",");
				temp2 = (int) (Long.parseLong(newt[0], 16)/Math.pow(2, 13));
				pt.pages.get(temp2).ins.add(count); //adds count to ins field
				//System.out.println("temp4 " + temp2);
				//System.out.println("in4 " + pt.pages.get(temp2).ins);
				count ++;
				//accesses ++;
			} else {
				temp = bf.readLine(); //next line
				continue;
			}
			temp = bf.readLine();
		}
		//close
		bf.close();
		input.close();
		return tempint;
	}
	
	/*
	 * This function runs the opt alg and calcs the correct number of pagefaults and writes to disk.
	 * Has future knowledge but impractical to implement. This means OS would know which page we won't use again
	 * Page w highest label should be evicted.
	 * 
	 * pagen - int that reps the page number
	 * p - pagetable that reps the page table
	 * num - int reps number of frames
	 * tracefile - String reps trace file
	 * count - int reps accesses
	 */
	public static void opt(int pagen, pagetable p, int num, String tracefile, int count){ //fine
		
		boolean evic = false;
		if (p.pages.get(pagen).val == false) {
			evic = true; //evict needs to occur
		}
		
		if (evic == true && p.map.size() == num) {
			//System.out.println("MAP " + p.map);
			//System.out.println("PG " + pagen);
			int t = -1;
			int index = 0;
			for (int i = 0; i < p.map.size(); i++) {
				//int one = p.map.get(i);
				page two = p.pages.get(p.map.get(i));
				int com;
				//assert two.ins.get(0) > count;
				if (two.ins.size() > 0) {
					com = two.ins.get(0); //get int val at ind 0
				} else {
					com = Integer.MAX_VALUE;
				}
				
				if (com > t) { //stores the bigger num
					t = com;
					index = i;
				}
			}
			p.pages.get(p.map.get(index)).val = false;
			if (p.pages.get(p.map.get(index)).dirt == true) {
				writesToDisk++;
			}
			p.pages.get(p.map.get(index)).dirt = false;
			//if (p.pages.get(p.map.get(index)).ins.size() > 0) {
			//	p.pages.get(p.map.get(index)).ins.remove(0);
			//}
			p.pages.get(pagen).val = true; // maps
			//System.out.println("in1 " + p.pages.get(p.map.get(index)).ins);
			//System.out.println("count " + count);
			//System.out.println("ind " + index);
			//System.out.println("MAP " + p.map);
			//assert p.pages.get(p.map.get(index)).ins.get(0) > count;
			p.map.remove(index); //remove map
			//System.out.println("in2 " + p.pages.get(p.map.get(index)).ins);
			//System.out.println("count2 " + count);
			//System.out.println("ind2 " + index);
			//System.out.println("MAP2 " + p.map);
			//numOfPageFaults++;
			//System.out.println("MAP " + p.map);
			//System.out.println("PG " + pagen);

		} //else {
		//assert p.pages.get(pagen).ins.get(0) == count;
			//p.pages.get(pagen).val = true; //maps
			if (p.pages.get(pagen).ins.size() > 0) {
				p.pages.get(pagen).ins.remove(0);
			}
		//}
		if (evic == true) {
			p.map.add(pagen);
			numOfPageFaults++; //incrememnt number of PF
			
		}
		
	}
	
	/*
	 * This function runs the clock alg and calcs the correct number of pagefaults and writes to disk.
	 * When PF occurs we inspect the page being pointed to by the hand. if R bit is false we evict and increment
	 * the hand. else advance hand to next page and inspect
	 * 
	 * pagen - int that reps the page number
	 * p - pagetable that reps the page table
	 * num - int reps number of frames
	 */
	public static void clock(int pagen, pagetable p, int num){ //fine
		boolean evic = false;
		if (p.pages.get(pagen).val == false) {
			evic = true; //need to evict
		}
		
		if (evic == true && p.map.size() == num) {
			while (true) { //finds correct page to mod
				if (p.pages.get(p.map.get(p.hand)).ref == true){ //page ref
					
					p.pages.get(p.map.get(p.hand)).ref = false;
					p.hand = (p.hand+1)%num; //hand adv
				} else {
					p.pages.get(p.map.get(p.hand)).val = false;
					if (p.pages.get(p.map.get(p.hand)).dirt == true) {
						writesToDisk ++; //dirty bit is 1
					}
					p.pages.get(p.map.get(p.hand)).dirt = false;
					p.pages.get(p.map.get(p.hand)).ref = false;
					p.pages.get(pagen).val = true; //maps
					p.map.remove(p.hand); //remove the map
					//numOfPageFaults++;
					break;
				}
				
			}
			
		} else {
			p.pages.get(pagen).val = true; //maps
		}
		if (evic == true) {
			p.map.add(pagen);
			numOfPageFaults++; //increments
		}
		
		//assert p.map.size() <= num;
		
	}

	/*
	 * This function runs the lru alg and calcs the correct number of pagefaults and writes to disk.
	 * Lru is least recently used so when PF occurs we examine the PT for the lowest counter.
	 * 
	 * pagen - int that reps the page number
	 * p - pagetable that reps the page table
	 * num - int reps number of frames
	 */
	public static void lru(int pagen, pagetable p, int num){ //fine
		boolean evic = false;
		if (p.pages.get(pagen).val == false) {
			evic = true; //we need to evict so set to true
		}
		//if we need to evict
		if (evic == true && p.map.size() == num) {
			int index = p.map.size()-1;
			int index2 = p.map.get(index);
			p.pages.get(index2).val = false;
			if (p.pages.get(index2).dirt == true) {
				writesToDisk++;
			}
			p.pages.get(index2).dirt = false;
			p.pages.get(pagen).val = true; // maps
			p.map.remove(index); //remove correct mapping
			// numOfPageFaults++;

		} else {
			p.pages.get(pagen).val = true; //maps
		}
		
		//adds to correct pos
		if (evic == true) {
			if (p.map.size() > 0) {
				p.map.add(0, pagen);
			} else {
				p.map.add(pagen); //add to front
			}
			numOfPageFaults++;
		} else {
			int temp = -1;
			for (int i = 0; i < p.map.size(); i++) {
				if (p.map.get(i) == pagen) {
					temp = i;
				}
			}
			//System.out.println("MAP " + p.map);
			//System.out.println("PG " + pagen);
			p.map.remove(temp); //remove correct page aka lru
			p.map.add(0, pagen); //add new page
			//System.out.println("MAP2 " + p.map);
			//if (p.map.size() > 0) {
				//p.map.remove(temp);
			//	System.out.println("MAP3 " + p.map);
			//}
			//System.out.println("MAP4 " + p.map);
		}
		/*
		ArrayList<Integer> newlist = p.map;
		for (int i = 0; i < newlist.size(); i++) {
			for (int j = 0; j < p.map.size(); j++) {
				if (newlist.get(i) == p.map.get(j) && i != j) {
					System.out.println(p.map);
				}
				
				
			}
		}*/
	}

	/*
	 * NRU wasn't part of the assignment. I confused it for NFU :/
	 */
	public static void nru(int pagen, pagetable p, int num){
		boolean evic = false;
		if (p.pages.get(pagen).val == false) {
			evic = true;
		}
		if (evic == true && p.map.size() == num) {
			//evic
			//System.out.println("HERE " + num);
			Random rand = new Random();
			//evoc old page
			int temp = rand.nextInt(num);
			int intRandom = p.map.get(temp);
		
			p.pages.get(intRandom).val = false;
			if (p.pages.get(intRandom).dirt == true) {
				writesToDisk ++;
			}
			p.pages.get(intRandom).dirt = false;
			p.pages.get(pagen).val = true;
			//assert p.pages.get(pagen).val == true;
			//System.out.println("HERE " + p.map);
			//System.out.println("Rand " + intRandom);
			p.map.remove(temp);
			//System.out.println("HERE2 " + p.map);
			//System.out.println("Rand2 " + intRandom);
			//numOfPageFaults++;
		} else {
			p.pages.get(pagen).val = true;
		}
		if (evic == true) {
			p.map.add(pagen);
			numOfPageFaults++;
		}
	}

	/*
	 * This function runs the nru alg and calcs the correct number of pagefaults and writes to disk.
	 * This alg works similar to LRU, but this alg doesn't forget anything. When PF occurs we evict
	 * the page w lowest counter. Main diff are the counters are shifted 1 bit to right and the ref bit is added leftmost. 
	 * 
	 * pagen - int that reps the page number
	 * p - pagetable that reps the page table
	 * num - int reps number of frames
	 */
	public static void nfu(int pagen, pagetable p, int num){
		//page newpage = null;
		//System.out.println("HERE");
		boolean evic = false;
		if (p.pages.get(pagen).val == false) {
			evic = true; //we need to evict
		}
		
		for (int i = 0; i < p.map.size(); i++) {
			int temp = p.map.get(i);
			//if (evic == true && p.map.get(i) != pagen) {
			//	temp = p.map.get(i);
			p.pages.get(temp).refc += (p.pages.get(temp).ref) ? 1:0; //if true add 1 else 0
			p.pages.get(temp).ref = false;
			//}
		}
		
		if (evic == true && p.map.size() == num) {
			int ref = Integer.MAX_VALUE;
			int ind = 0;
			ArrayList<Integer> newmap = new ArrayList<Integer>();
			for (int i = 0; i < p.map.size(); i++) {
				int temp = p.map.get(i);
				int com = p.pages.get(temp).refc;
				newmap.add(com);
				if (com < ref) { //finds bigger num
					ind = i;
					ref = com;
				}
			}
			//System.out.println("NEW " + newmap);
			//System.out.println("IND " + ind);
			p.pages.get(p.map.get(ind)).val = false;
			if (p.pages.get(p.map.get(ind)).dirt == true) {
				writesToDisk ++; //increments
			}
			p.pages.get(p.map.get(ind)).dirt = false;
			p.pages.get(p.map.get(ind)).ref = false;
			//p.pages.get(p.map.get(ind)).refc = 0;
			p.pages.get(pagen).val = true; //maps
			p.pages.get(pagen).refc ++;
			//assert p.pages.get(pagen).val == true;
			//System.out.println("MAP " + p.map);
			//System.out.println("PG " + ind);
			p.map.remove(ind); //remove correct item in map
			//System.out.println("MAP2 " + p.map);
			//System.out.println("PG2 " + ind);
			
		}
		if (evic == true) {
			p.pages.get(pagen).val = true;
			//p.pages.get(pagen).ref = false;
			p.map.add(pagen); //add new page map
			numOfPageFaults++;
		}
	}
	

}
