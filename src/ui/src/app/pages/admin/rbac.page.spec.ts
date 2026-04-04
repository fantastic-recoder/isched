import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { RbacPage } from './rbac.page';

describe('RbacPage', () => {
  let httpMock: HttpTestingController;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [RbacPage],
      providers: [provideHttpClient(), provideHttpClientTesting()],
    }).compileComponents();

    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    httpMock.verify();
  });

  it('loads roles on init', () => {
    const fixture = TestBed.createComponent(RbacPage);
    fixture.detectChanges();

    httpMock.expectOne('/graphql').flush({
      data: { roles: [{ id: 'role_user', name: 'User', scope: 'tenant' }] },
    });

    expect(fixture.componentInstance.roles().length).toBe(1);
  });
});

