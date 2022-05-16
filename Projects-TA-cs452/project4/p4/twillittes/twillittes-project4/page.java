import java.util.ArrayList;
/*
 * Taylor Willittes
 * 452 Proj 4
 * File: page.java
 * Puropse: small class to keep track of properties of a page such as valid/dirty/refrenced bit, refrenced count 
 * and an arraylist of instructions
 * Usage: vmsim –n <numframes> -a <opt|clock|lru|nfu> <tracefile>
 */
public class page {

	//globs
	boolean val = false;
	boolean dirt = false;
	boolean ref = false;
	int refc = 0;
	ArrayList<Integer> ins = new ArrayList<Integer>();
	//int last = 0;

	/*
	 * defualt construct
	 */
	public page() {

	}
}
