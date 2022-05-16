import java.util.ArrayList;

/*
 * Taylor Willittes
 * 452 Proj 4
 * File: pagetable.java
 * Puropse: compare/contrast 4 diff page replacement algs w the num of PF and WTD.
 * This file keeps track of page table properties
 * Usage: vmsim –n <numframes> -a <opt|clock|lru|nfu> <tracefile>
 */
public class pagetable {

	//globs
	ArrayList<page> pages = new ArrayList<page>();
	ArrayList<Integer> map = new ArrayList<Integer>();
	int hand = 0;
	
	/*
	 * constructor that initializes the pagetable
	 * 
	 * pagefr - int that reps the page frame
	 */
	public pagetable(int pagefr){
	    
	    for (int i = 0; i < (Math.pow(2, 32)/Math.pow(2, 13)); i++) { //11
	    	pages.add(new page()); //adds correct num of pages to pages array
	    }
	    
	    //System.out.println("map " + map.size());
	    
	  }
	
	/*
	 * This function accesses mem based on the instruction given.
	 * 
	 * op - String that reps the instruction we read from the input file
	 * loc - Long that reps the location of page
	 */
	public int accessMemory(String op, Long loc) {
    	int pagenum = (int) (loc/Math.pow(2, 13)); //gets correct page num
    	//System.out.println("PAGE " + pagenum);
    	if (pagenum >= (int) (Math.pow(2, 32)/Math.pow(2, 13))){ //error check
    		System.out.println("ERROR1");
    	}
    	//int ev = 0;
    	//sets correct bits for each page if we read in an M or S
    	if (op.equals("M") || op.equals("S")) {
    		//System.out.println("HERE");
    		pages.get(pagenum).dirt = true;
    	}
    		pages.get(pagenum).ref = true; //refrenced bit true
    	//}
    	//if (pages.get(pagenum).val == true && !map.contains(pagenum)) { //error check
    		//for (int i = 0;)
    	//	System.out.println("ERROR");
    	//}
    	
    	return pagenum;
    }

}
