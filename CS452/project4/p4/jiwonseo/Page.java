public class Page{
    int index;
    int frame;
    boolean valid;
    boolean dirty;
    boolean ref;
    int age;
    int timestamp;
    String address;

    public Page(){
        this.index = 0;
        this.frame=-1;
        this.valid=false;
        this.dirty =false;
        this.ref =false;
        this.age = 0;
        this.timestamp=-1;
        this.address="";
    }

    public Page(Page entry){
        this.index = entry.index;
        this.frame = entry.frame;
        this.valid = entry.valid;
        this.dirty = entry.dirty;
        this.ref = entry.ref;
        this.age = entry.age;
        this.timestamp=entry.timestamp;
        this.address = entry.address;
        
    }
    
}